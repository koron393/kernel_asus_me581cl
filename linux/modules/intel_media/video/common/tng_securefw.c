/**************************************************************************
 * Copyright (c) 2014, Intel Corporation.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 **************************************************************************/

#include <linux/firmware.h>
#include <drm/drmP.h>
#include <drm/drm.h>
#include "psb_drv.h"
#include "vsp.h"


#ifdef CONFIG_DX_SEP54
extern int sepapp_image_verify(u8 *addr, ssize_t size, u32 key_index, u32 magic_num);
#endif
extern struct soft_platform_id spid;
extern int IFWI_minor_version;

/*
 * For a new product/device, if the device information is not in the spid2fw
 * table, driver will
 * 1) Firstly try to load firmware with the name like msvdx.bin.0004.0004.000d
 * 2) If there is no such firmware, driver will use the existing firmware from
 *    the closest device (usually with same family id or product id)
 * 3) Use the key from the closest device
 *
 * If above 1)/2) is failed, a new firmware is needed. For testing pupose without
 * driver change
 * 1) The new firmware must be with the name like msvdx.bin.0004.0004.000d
 * 2) If use other key, try to modify /sys/module/<driver module>/video_sepkey
 *
 * For the formal change, extend spid2fw table to include the device and firmware
 */

#define FW_NAME_LEN  64
#define IMR_REG_NUMBER(imrl_reg) ((imrl_reg - TNG_IMR_MSG_REGBASE) >> 2)
#define ISLAND_MAGIC_NUMBER(island_str) ((island_str[2] << 24) | (island_str[1] << 16) | (island_str[0] << 8) | '$')

/*
 * Common firmware files shared by differnt product/hardware
 */
#define mrfl_ffrd_pr2_b0_msvdx	"msvdx.bin.0004.0000.000d"
#define mrfl_vv_fab_b_b0_msvdx	"msvdx.bin.0004.0000.0001"
#define mrfl_vv_msvdx		mrfl_vv_fab_b_b0_msvdx
#define mrfl_product_msvdx	"msvdx.bin.0004.0000.0015"
#define mrfl_pr2_macro_b0_msvdx	"msvdx.bin.0004.0000.000c"
#define mrfl_sand_msvdx		"msvdx.bin.0004.0002.0001"
#define mofd_vv_fab_a_msvdx	"msvdx.bin.0008.0000.0000"
#define mofd_v0_msvdx		"msvdx.bin.0008.0000.0001"
#define mofd_v1_pr2_msvdx	"msvdx.bin.0008.0002.0001"
#define mofd_ffrd_pr0_msvdx	mofd_v0_msvdx
//#define mofd_product_msvdx	"mofd_product_msvdx.bin"
#define mofd_prh_b0_msvdx	"msvdx.bin.000c.0001.0001"
#define mofd_QS_msvdx	"msvdx.bin.0008.0000.0002"

#define mrfl_ffrd_pr2_b0_topaz	"topaz.bin.0004.0000.000d"
#define mrfl_vv_fab_b_b0_topaz	"topaz.bin.0004.0000.0001"
#define mrfl_vv_topaz		mrfl_vv_fab_b_b0_topaz
#define mrfl_pr2_macro_b0_topaz	"topaz.bin.0004.0000.000c"
#define mrfl_product_topaz	"topaz.bin.0004.0000.0015"
#define mrfl_sand_topaz		"topaz.bin.0004.0002.0001"
#define mofd_vv_fab_a_topaz	"topaz.bin.0008.0000.0000"
#define mofd_v0_topaz		"topaz.bin.0008.0000.0001"
#define mofd_v1_pr2_topaz	"topaz.bin.0008.0002.0001"
#define mofd_ffrd_pr0_topaz	mofd_vv_fab_a_topaz
//#define mofd_product_topaz	"mofd_product_topaz.bin"
#define mofd_prh_b0_topaz	"topaz.bin.000c.0001.0001"
#define mofd_QS_topaz	"topaz.bin.0008.0000.0002"

#define mrfl_ffrd_pr2_b0_vsp	"vsp.bin.0004.0000.000d"
#define mrfl_vv_fab_b_b0_vsp	"vsp.bin.0004.0000.0001"
#define mrfl_vv_vsp		mrfl_vv_fab_b_b0_vsp
#define mrfl_product_vsp	"vsp.bin.0004.0000.0015"
#define mrfl_sand_vsp		"vsp.bin.0004.0002.0001"
#define mrfl_pr2_macro_b0_vsp	"vsp.bin.0004.0000.000c"
#define mofd_vv_fab_a_vsp	"vsp.bin.0008.0000.0000"
#define mofd_v0_vsp		"vsp.bin.0008.0000.0001"
#define mofd_v1_pr2_vsp		"vsp.bin.0008.0002.0001"
#define mofd_ffrd_pr0_vsp	mofd_vv_fab_a_vsp
//#define mofd_product_vsp	"mofd_product_vsp.bin"
#define mofd_prh_b0_vsp		"vsp.bin.000c.0001.0001"
#define mofd_QS_vsp		"vsp.bin.0008.0000.0002"

#define mrfl_defalut_msvdx	mrfl_product_msvdx
#define mrfl_defalut_topaz	mrfl_product_topaz
#define mrfl_defalut_vsp	mrfl_product_vsp

#define mofd_defalut_msvdx	mofd_v0_msvdx
#define mofd_defalut_topaz	mofd_v0_topaz
#define mofd_defalut_vsp	mofd_v0_vsp

/*
 * Firmware name if there is no entry in spid2fw table
 */
static char default_msvdx[FW_NAME_LEN];
static char default_topaz[FW_NAME_LEN];
static char default_vsp[FW_NAME_LEN];

struct spid2fw_mapping {
	u16 family_id;
	u16 product_id;
	u16 hardware_id;
	char *msvdx_fw;
	char *topaz_fw;
	char *vsp_fw;
	int sep_key;
};

/*
 * Table spid2fw
 */
static struct spid2fw_mapping spid2fw[] = {
	{4, 0, 0, mrfl_vv_msvdx, mrfl_vv_topaz, mrfl_vv_vsp, 15}, /* merrifield VV */
	{4, 0, 1, mrfl_vv_fab_b_b0_msvdx, mrfl_vv_fab_b_b0_topaz, mrfl_vv_fab_b_b0_vsp, 15}, /* merrifield VV Fab B (B0) */
	//{4, 0, 0xc, mrfl_pr2_macro_b0_msvdx, mrfl_pr2_macro_b0_topaz, mrfl_pr2_macro_b0_vsp, 15}, /* merrifield PR2 Macro (B0) */
	{4, 0, 0xd, mrfl_ffrd_pr2_b0_msvdx, mrfl_ffrd_pr2_b0_topaz, mrfl_ffrd_pr2_b0_vsp, 15}, /* merrifield FFRD PR2 (B0) */
	{4, 0, 15, mrfl_product_msvdx, mrfl_product_topaz, mrfl_product_vsp, 15}, /* merrifield product */
	{4, 2, 1, mrfl_sand_msvdx, mrfl_sand_topaz, mrfl_sand_vsp, 0}, /* merrifield SAND */

	{8, 0, 0, mofd_vv_fab_a_msvdx, mofd_vv_fab_a_topaz, mofd_vv_fab_a_vsp, 15}, /* moorefield VV Fab A */

	{8, 0, 2, mofd_QS_msvdx, mofd_QS_topaz, mofd_QS_vsp, 15}, /* mofd QS SoC*/

	{8, 0, 1, mofd_v0_msvdx, mofd_v0_topaz, mofd_v0_vsp, 15}, /* moorefield V0 */
	{8, 2, 0, mofd_vv_fab_a_msvdx, mofd_vv_fab_a_topaz, mofd_vv_fab_a_vsp, 15}, /* moorefield V1 VV with A0 soc */
	{8, 2, 1, mofd_v1_pr2_msvdx, mofd_v1_pr2_topaz, mofd_v1_pr2_vsp, 15}, /* moorefield V1 PR2 */
	//{8, 0, 2, mofd_product_msvdx, mofd_product_topaz, mofd_product_vsp, 15}, /* Anniedale Production Keys (QS/PRQ) */
	{0xc, 0, 4, mofd_ffrd_pr0_msvdx, mofd_ffrd_pr0_topaz, mofd_ffrd_pr0_vsp, 15}, /* moorefield FFRD PR0 */
	{0xc, 1, 1, mofd_prh_b0_msvdx, mofd_prh_b0_topaz, mofd_prh_b0_vsp, 15}, /* MCG Moorefield PRH B0 */

	{-1, -1, -1, NULL, NULL, NULL, 15} /* the last entry for non-supported device */
};

/*
* Fall back once mismatch
*/
static struct spid2fw_mapping default_mrfl_spid2fw =
	{4, 0, 15, mrfl_product_msvdx, mrfl_product_topaz, mrfl_product_vsp, 15}; /* merrifield product */

static struct spid2fw_mapping default_mofd_spid2fw =
	{8, 0, 1, mofd_v0_msvdx, mofd_v0_topaz, mofd_v0_vsp, 15}; /* moorefield V0 */

#define SPID2FW_NUMBER sizeof(spid2fw)/sizeof(struct spid2fw_mapping)
static struct spid2fw_mapping *matched_spid2fw = NULL; /* query once, use multiple times */

/*
 * There is a matched entry in the table
 */
#define ID_MATCH(_spid, _spid2fw) ((_spid.platform_family_id == _spid2fw->family_id) && \
				   (_spid.product_line_id == _spid2fw->product_id) && \
				   (_spid.hardware_id == _spid2fw->hardware_id))

static void tng_copyfw(char *fw_name, char *island_name, int *sep_key, struct spid2fw_mapping *p)
{
	if (!strncmp(island_name, "VED", 3))
		strcpy(fw_name, p->msvdx_fw);
	else if (!strncmp(island_name, "VEC", 3))
		strcpy(fw_name, p->topaz_fw);
	else if (!strncmp(island_name, "VSP", 3))
		strcpy(fw_name, p->vsp_fw);
	
	if (drm_video_sepkey == -1)
		*sep_key = p->sep_key;
	else
		*sep_key = drm_video_sepkey;

	return;
}

static void tng_spid2fw(struct drm_device *dev, char *fw_name, char *fw_basename, char *island_name, int *sep_key)
{
	struct spid2fw_mapping *p;
	const struct firmware *raw = NULL;
	int i, ret;

	pr_info("%s: IFWI_minor_version=%x\n", __func__, IFWI_minor_version);

	if(IFWI_minor_version & 1 == 1) {
		pr_info("%s: QS device, change spid.hardware_id to 2 !!\n", __func__);
		spid.hardware_id = 2;
	}
	else
		pr_info("%s: preQS device, keep spid unchanged !!\n", __func__);

	/* aready get the matched entry in spid2fw table */
	if (matched_spid2fw) {
		tng_copyfw(fw_name, island_name, sep_key, matched_spid2fw);
		return;
	}

	/* try to find the matched entry in spid2fw table */
	p = spid2fw;
	for (i = 0; i < SPID2FW_NUMBER; i++) {
		if (ID_MATCH(spid, p)) {
			matched_spid2fw = p;
			break;
		}

		p++;
	}

	/* find a matched entry */
	if (matched_spid2fw != NULL) {
		tng_copyfw(fw_name,  island_name, sep_key, matched_spid2fw);
		DRM_INFO("Got matched firmware %s\n", fw_name, fw_name);
		return;
	}

	/*
	 * No entry in the table, fake one
	 * Firstly try if we have the firmware like mvsdx.bin.0004.0002.000d
	 */
	DRM_ERROR("Cannot find pre-defined firmware for this spid, try to detect %s firmware\n", fw_basename);

	snprintf(fw_name, FW_NAME_LEN, "%s.bin.%04x.%04x.%04x",
		fw_basename,
		(int)spid.platform_family_id,
		(int)spid.product_line_id,
		(int)spid.hardware_id);

	ret = request_firmware(&raw, fw_name, &dev->pdev->dev);
	if (raw == NULL || ret < 0) { /* there is no mvsdx.bin.0004.0002.000d */
		DRM_ERROR("There is no firmware %s, try to use the default device firmware\n", fw_name);

		if ((int)spid.platform_family_id == 4)
			matched_spid2fw = &default_mrfl_spid2fw;
		else
			matched_spid2fw = &default_mofd_spid2fw;

		tng_copyfw(fw_name,  island_name, sep_key, matched_spid2fw);
		return;
	}
	/*
	 * We have firmware like mvsdx.bin.0004.0002.000d
	 * Fake one entry in the table for other islands
	 */
	release_firmware(raw);

	PSB_DEBUG_INIT("Detect %s firmware %s success! Fake entries for all the other islands\n", fw_basename, fw_name);

	matched_spid2fw = &spid2fw[SPID2FW_NUMBER - 1];

	matched_spid2fw->msvdx_fw = default_msvdx;
	snprintf(matched_spid2fw->msvdx_fw, FW_NAME_LEN, "msvdx.bin.%04x.%04x.%04x",
		 (int)spid.platform_family_id, (int)spid.product_line_id, (int)spid.hardware_id);
	
	matched_spid2fw->topaz_fw = default_topaz;
	snprintf(matched_spid2fw->topaz_fw, FW_NAME_LEN, "topaz.bin.%04x.%04x.%04x",
		 (int)spid.platform_family_id, (int)spid.product_line_id, (int)spid.hardware_id);
	
	matched_spid2fw->vsp_fw = default_vsp;
	snprintf(matched_spid2fw->vsp_fw, FW_NAME_LEN, "vsp.bin.%04x.%04x.%04x",
		 (int)spid.platform_family_id, (int)spid.product_line_id, (int)spid.hardware_id);

	/* force the sep key with the nearest one*/
	if ((int)spid.platform_family_id == 4)
		matched_spid2fw->sep_key =  default_mrfl_spid2fw.sep_key;
	else
		matched_spid2fw->sep_key =  default_mofd_spid2fw.sep_key;

	tng_copyfw(fw_name, island_name, sep_key, matched_spid2fw);
	return;
}

static void tng_get_fwinfo(struct drm_device *dev, char *fw_name, char *fw_basename, char *island_name, int *sep_key)
{
	DRM_INFO("SPID: family_id.product_id.hardware_id=0x%04x.0x%04x.0x%04x\n",
		       spid.platform_family_id, spid.product_line_id, spid.hardware_id);
#if 0
	snprintf(fw_name, FW_NAME_LEN, "%s.bin.%04x.%04x",
		 fw_basename, (int)spid.platform_family_id, (int)spid.hardware_id);

	if (drm_video_sepkey == -1)
		*sep_key = 15;
	else
		*sep_key = drm_video_sepkey;
	/* silent the warning */
	(void)tng_spid2fw;
	
	return;
#else
	tng_spid2fw(dev, fw_name, fw_basename, island_name, sep_key);
#endif
	
	PSB_DEBUG_INIT("Use firmware %s for %s, SEP key %d\n", fw_name, island_name, *sep_key);
}

static void tng_print_imrinfo(int imrl_reg, uint64_t imr_base, uint64_t imr_end)
{
	unsigned int imr_regnum = IMR_REG_NUMBER(imrl_reg);
	
	if (imr_base != 0)
		PSB_DEBUG_INIT("IMR%d ranges 0x%12llx - 0x%12llx\n",
			       imr_regnum, imr_base, imr_end);

	PSB_DEBUG_INIT("IMR%d L 0x%2x = 0x%032x\n",
		       imr_regnum, imrl_reg,
		       intel_mid_msgbus_read32(PNW_IMR_MSG_PORT, imrl_reg));
	PSB_DEBUG_INIT("IMR%d H 0x%2x = 0x%032x\n",
		       imr_regnum, imrl_reg + 1,
		       intel_mid_msgbus_read32(PNW_IMR_MSG_PORT, imrl_reg+1));
	PSB_DEBUG_INIT("IMR%d RAC 0x%2x = 0x%032x\n",
		       imr_regnum,  imrl_reg + 2,
		       intel_mid_msgbus_read32(PNW_IMR_MSG_PORT, imrl_reg+2));
	PSB_DEBUG_INIT("IMR%d WAC 0x%2x = 0x%032x\n",
		       imr_regnum, imrl_reg + 3,
		       intel_mid_msgbus_read32(PNW_IMR_MSG_PORT, imrl_reg+3));
}

static void tng_get_imrinfo(int imrl_reg, uint64_t *imr_addr)
{
	uint32_t imrl, imrh;
	uint64_t imr_base, imr_end;
	
	imrl = intel_mid_msgbus_read32(TNG_IMR_MSG_PORT, imrl_reg);
	imrh = intel_mid_msgbus_read32(TNG_IMR_MSG_PORT, imrl_reg+1);

	imr_base = (imrl & TNG_IMR_ADDRESS_MASK) << TNG_IMR_ADDRESS_SHIFT;
	imr_end = (imrh & TNG_IMR_ADDRESS_MASK) << TNG_IMR_ADDRESS_SHIFT;

	*imr_addr = imr_base;
	
	tng_print_imrinfo(imrl_reg, imr_base, imr_end);
}

static int tng_securefw_prevsp(struct drm_device *dev, const struct firmware *raw)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;
	struct vsp_secure_boot_header *boot_header;
	struct vsp_multi_app_blob_data *ma_header;
	unsigned int vrl_header_size = 736;
	unsigned char *ptr, *ma_ptr;
	
	if (raw->size < sizeof(struct vsp_secure_boot_header)) {
		DRM_ERROR("VSP:firmware is not a correct firmware (size %d)\n", (int)raw->size);
		return 1;
	}

	ptr = (void *)raw->data;
	ma_ptr = (void *) raw->data + vrl_header_size;
	boot_header = (struct vsp_secure_boot_header *)(ptr + vrl_header_size);
	ma_header = (struct vsp_multi_app_blob_data *)(ma_ptr + boot_header->ma_header_offset);

	/* get firmware header */
	memcpy(&vsp_priv->boot_header, boot_header, sizeof(vsp_priv->boot_header));

	if (vsp_priv->boot_header.magic_number != VSP_SECURE_BOOT_MAGIC_NR) {
		DRM_ERROR("VSP: failed to load correct vsp firmware\n"
			  "FW magic number is wrong %x (should be %x)\n",
			  vsp_priv->boot_header.magic_number,
			  VSP_SECURE_BOOT_MAGIC_NR);
		return 1;
	}

	/* read application firmware image data (for state-buffer size, etc) */
	/* load the multi-app blob header */
	memcpy(&vsp_priv->ma_header, ma_header, sizeof(vsp_priv->ma_header));
	if (vsp_priv->ma_header.magic_number != VSP_MULTI_APP_MAGIC_NR) {
		DRM_ERROR("VSP: failed to load correct vsp firmware\n"
			  "FW magic number is wrong %x (should be %x)\n",
			  vsp_priv->ma_header.magic_number,
			  VSP_MULTI_APP_MAGIC_NR);

		return 1;
	}

	VSP_DEBUG("firmware secure header:\n");
	VSP_DEBUG("boot_header magic number %x\n", boot_header->magic_number);
	VSP_DEBUG("boot_text_offset %x\n", boot_header->boot_text_offset);
	VSP_DEBUG("boot_text_reg %x\n", boot_header->boot_text_reg);
	VSP_DEBUG("boot_icache_value %x\n", boot_header->boot_icache_value);
	VSP_DEBUG("boot_icache_reg %x\n", boot_header->boot_icache_reg);
	VSP_DEBUG("boot_pc_value %x\n", boot_header->boot_pc_value);
	VSP_DEBUG("boot_pc_reg %x\n", boot_header->boot_pc_reg);
	VSP_DEBUG("ma_header_offset %x\n", boot_header->ma_header_offset);
	VSP_DEBUG("ma_header_reg %x\n", boot_header->ma_header_reg);
	VSP_DEBUG("boot_start_value %x\n", boot_header->boot_start_value);
	VSP_DEBUG("boot_start_reg %x\n", boot_header->boot_start_reg);
	VSP_DEBUG("firmware ma_blob header:\n");
	VSP_DEBUG("ma_header magic number %x\n", ma_header->magic_number);
	VSP_DEBUG("offset_from_start %x\n", ma_header->offset_from_start);
	VSP_DEBUG("imr_state_buffer_addr %x\n", ma_header->imr_state_buffer_addr);
	VSP_DEBUG("imr_state_buffer_size %x\n", ma_header->imr_state_buffer_size);
	VSP_DEBUG("apps_default_context_buffer_size %x\n",
		  ma_header->apps_default_context_buffer_size);

	return 0;
}

static void tng_securefw_postvsp(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;

	vsp_priv->ctrl = (struct vsp_ctrl_reg *) (dev_priv->vsp_reg +
						  VSP_CONFIG_REG_SDRAM_BASE +
						  VSP_CONFIG_REG_START);

	vsp_priv->fw_loaded = VSP_FW_LOADED;
	vsp_priv->vsp_state = VSP_STATE_DOWN;

}

int tng_securefw(struct drm_device *dev, char *fw_basename, char *island_name, int imrl_reg)
{
	const struct firmware *raw = NULL;
	char fw_name[FW_NAME_LEN];
	unsigned char *imr_ptr;
	uint64_t imr_addr;
	int ret = 0, sep_key, fw_size;
	
	tng_get_fwinfo(dev, fw_name, fw_basename, island_name, &sep_key);

	/* try to load firmware from storage */
	PSB_DEBUG_INIT("Try to request firmware %s\n", fw_name);
	ret = request_firmware(&raw, fw_name, &dev->pdev->dev);
	if (raw == NULL || ret < 0) {
		DRM_ERROR("Failed to request firmware %s, ret =  %d\n", fw_name, ret);
		return ret;
	}

	if (!strncmp(island_name, "VSP", 3)) {
		ret = tng_securefw_prevsp(dev, raw);
		if (ret) {
			DRM_ERROR("VSP sanity check failed\n");
			release_firmware(raw);
			return ret;
		}
	}
	
	PSB_DEBUG_INIT("Try to get IMR region information\n");
	tng_get_imrinfo(imrl_reg, &imr_addr);

	PSB_DEBUG_INIT("Try to map IMR region\n");
	imr_ptr = ioremap(imr_addr, raw->size);
	if (!imr_ptr) {
		DRM_ERROR("Failed to map IMR region\n");
		release_firmware(raw);
		return 1;
	}
	
	fw_size = raw->size;
	PSB_DEBUG_INIT("Try to copy firmware into IMR region\n");
	memcpy(imr_ptr, raw->data, fw_size);

	PSB_DEBUG_INIT("Try to unmap IMR region\n");	
	iounmap(imr_ptr);

	PSB_DEBUG_INIT("Try to release firmware\n");		
	release_firmware(raw);

#ifdef CONFIG_DX_SEP54
	PSB_DEBUG_INIT("Try to verify firmware\n");	
	ret = sepapp_image_verify((u8 *)imr_addr, fw_size, sep_key,
				  ISLAND_MAGIC_NUMBER(island_name));
	if (ret) {
		DRM_ERROR("Failed to verify firmware %x\n", ret);
		return ret;
	}
	PSB_DEBUG_INIT("After verification, IMR region information\n");
	tng_print_imrinfo(imrl_reg, 0, 0);
#endif
	
	if (!strncmp(island_name, "VSP", 3))
		tng_securefw_postvsp(dev);
	
	return ret;
}

int tng_rawfw(struct drm_device *dev, uint8_t *fw_basename)
{
	DRM_ERROR("Non secure mode never be ran\n");
	
	return 1;
}

