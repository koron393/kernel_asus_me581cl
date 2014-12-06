/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "platform_support.h"
#include "sh_css_firmware.h"

#include "sh_css_defs.h"
#include "ia_css_debug.h"
#include "sh_css_internal.h"
#include "ia_css_isp_param.h"

#include "memory_access.h"
#include "assert_support.h"

#include "isp.h"				/* PMEM_WIDTH_LOG2 */

#include "ia_css_isp_params.h"
#include "ia_css_isp_configs.h"
#include "ia_css_isp_states.h"

#define _STR(x) #x
#define STR(x) _STR(x)

struct firmware_header {
	struct sh_css_fw_bi_file_h file_header;
	struct ia_css_fw_info      binary_header;
};

struct fw_param {
	const char *name;
	const void *buffer;
};

/* Warning: same order as SH_CSS_BINARY_ID_* */
static struct firmware_header *firmware_header;

/* The string STR(irci_master_r44c_20140704_0226) is a place holder
 * which will be replaced with the actual RELEASE_VERSION
 * during package generation. Please do not modify  */
static const char *release_version = STR(irci_master_r44c_20140704_0226);

#define MAX_FW_REL_VER_NAME	300
static char FW_rel_ver_name[MAX_FW_REL_VER_NAME] = "---";

struct ia_css_fw_info	  sh_css_sp_fw;
#if defined(ENABLE_SP1)
struct ia_css_fw_info	  sh_css_sp1_fw;
#endif
struct ia_css_blob_descr *sh_css_blob_info; /* Only ISP blob info (no SP) */
unsigned		  sh_css_num_binaries; /* This includes 1 SP binary */

static struct fw_param *fw_minibuffer;


char *sh_css_get_fw_version(void)
{
	return(FW_rel_ver_name);
}


/*
 * Split the loaded firmware into blobs
 */

/* Setup sp/sp1 binary */
static enum ia_css_err
setup_sp(struct ia_css_fw_info *fw, const char *fw_data, struct ia_css_fw_info *sh_css_sp_sp1_fw, unsigned sp_id)
{
	const char *blob_data;

	assert(fw != NULL);
	assert(fw_data != NULL);

	blob_data = fw_data + fw->blob.offset;

	*sh_css_sp_sp1_fw = *fw;

#if defined(C_RUN) || defined(HRT_UNSCHED)
	sh_css_sp_sp1_fw->blob.code = sh_css_malloc(1);
#else
	sh_css_sp_sp1_fw->blob.code = sh_css_malloc(fw->blob.size);
#endif

	if (sh_css_sp_sp1_fw->blob.code == NULL)
		return IA_CSS_ERR_CANNOT_ALLOCATE_MEMORY;

	memcpy((void *)sh_css_sp_sp1_fw->blob.code, blob_data, fw->blob.size);
	sh_css_sp_sp1_fw->blob.data = (char *)sh_css_sp_sp1_fw->blob.code + fw->blob.data_source;
	fw_minibuffer[sp_id].buffer = sh_css_sp_sp1_fw->blob.code;

	return IA_CSS_SUCCESS;
}
enum ia_css_err
sh_css_load_blob_info(const char *fw, const struct ia_css_fw_info *bi, struct ia_css_blob_descr *bd, unsigned index)
{
	const char *name;
	const unsigned char *blob;

	assert(fw != NULL);
	assert(bd != NULL);

	/* Special case: only one binary in fw */
	if (bi == NULL) bi = (const struct ia_css_fw_info *)fw;

	name = (const char *)fw + bi->blob.prog_name_offset;
	blob = (const unsigned char *)fw + bi->blob.offset;

	/* sanity check */
	if (bi->blob.size != bi->blob.text_size + bi->blob.icache_size + bi->blob.data_size + bi->blob.padding_size) {
		/* sanity check, note the padding bytes added for section to DDR alignment */
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	}

	if ((bi->blob.offset % (1UL<<(ISP_PMEM_WIDTH_LOG2-3))) != 0)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	bd->blob = blob;
	bd->header = *bi;

	if ((bi->type == ia_css_isp_firmware) || (bi->type == ia_css_sp_firmware)
#if defined(ENABLE_SP1)
	|| (bi->type == ia_css_sp1_firmware)
#endif
	)
	{
		char *namebuffer;
		int namelength = (int)strlen(name);

		namebuffer = (char *) sh_css_malloc(namelength+1);
		if (namebuffer == NULL)
			return IA_CSS_ERR_CANNOT_ALLOCATE_MEMORY;

		memcpy(namebuffer, name, namelength+1);

		bd->name = fw_minibuffer[index].name = namebuffer;
	} else {
		bd->name = name;
	}

	if (bi->type == ia_css_isp_firmware) {
		size_t paramstruct_size = sizeof(struct ia_css_memory_offsets);
		size_t configstruct_size = sizeof(struct ia_css_config_memory_offsets);
		size_t statestruct_size = sizeof(struct ia_css_state_memory_offsets);

		char *parambuf = (char *) sh_css_malloc(paramstruct_size + configstruct_size + statestruct_size);
		if (parambuf == NULL)
			return IA_CSS_ERR_CANNOT_ALLOCATE_MEMORY;

		bd->mem_offsets.array[IA_CSS_PARAM_CLASS_PARAM].ptr = NULL;
		bd->mem_offsets.array[IA_CSS_PARAM_CLASS_CONFIG].ptr = NULL;
		bd->mem_offsets.array[IA_CSS_PARAM_CLASS_STATE].ptr = NULL;

		fw_minibuffer[index].buffer = parambuf;

		/* copy ia_css_memory_offsets */
		memcpy(parambuf, (void *)(fw + bi->blob.memory_offsets.offsets[IA_CSS_PARAM_CLASS_PARAM]),
			paramstruct_size);
		bd->mem_offsets.array[IA_CSS_PARAM_CLASS_PARAM].ptr = parambuf;

		/* copy ia_css_config_memory_offsets */
		memcpy(parambuf + paramstruct_size,
				(void *)(fw + bi->blob.memory_offsets.offsets[IA_CSS_PARAM_CLASS_CONFIG]),
				configstruct_size);
		bd->mem_offsets.array[IA_CSS_PARAM_CLASS_CONFIG].ptr = parambuf + paramstruct_size;

		/* copy ia_css_state_memory_offsets */
		memcpy(parambuf + paramstruct_size + configstruct_size,
				(void *)(fw + bi->blob.memory_offsets.offsets[IA_CSS_PARAM_CLASS_STATE]),
				statestruct_size);
		bd->mem_offsets.array[IA_CSS_PARAM_CLASS_STATE].ptr = parambuf + paramstruct_size + configstruct_size;
	}
	return IA_CSS_SUCCESS;
}

enum ia_css_err
sh_css_load_firmware(const char *fw_data,
		     unsigned int fw_size)
{
	unsigned i;
	struct ia_css_fw_info *binaries;
	struct sh_css_fw_bi_file_h *file_header;

	firmware_header = (struct firmware_header *)fw_data;
	file_header = (struct sh_css_fw_bi_file_h *)&firmware_header->file_header;
	binaries = (struct ia_css_fw_info *)&firmware_header->binary_header;
	strcpy(FW_rel_ver_name, file_header->version);
	if (strcmp(file_header->version, release_version) != 0) {
#if (!defined HRT_CSIM && !defined HRT_RTL)
		IA_CSS_ERROR("CSS code and firmware version mismatch!");
		return IA_CSS_ERR_VERSION_MISMATCH;
#endif
	} else {
		IA_CSS_LOG("successfully load firmware version %s", release_version);
	}

	/* some sanity checks */
	if (!fw_data || fw_size < sizeof(struct sh_css_fw_bi_file_h))
		return IA_CSS_ERR_INTERNAL_ERROR;

	if (file_header->h_size != sizeof(struct sh_css_fw_bi_file_h))
		return IA_CSS_ERR_INTERNAL_ERROR;

	sh_css_num_binaries = file_header->binary_nr;
	/* Only allocate memory for ISP blob info */
	if (sh_css_num_binaries > NUM_OF_SPS) {
		sh_css_blob_info = sh_css_malloc(
					(sh_css_num_binaries - NUM_OF_SPS) *
					sizeof(*sh_css_blob_info));
		if (sh_css_blob_info == NULL)
			return IA_CSS_ERR_CANNOT_ALLOCATE_MEMORY;
	} else {
		sh_css_blob_info = NULL;
	}

	fw_minibuffer = sh_css_malloc(sh_css_num_binaries * sizeof(struct fw_param));
	if (fw_minibuffer == NULL)
		return IA_CSS_ERR_CANNOT_ALLOCATE_MEMORY;
	memset(fw_minibuffer, 0, sh_css_num_binaries * sizeof(struct fw_param));

	for (i = 0; i < sh_css_num_binaries; i++) {
		struct ia_css_fw_info *bi = &binaries[i];
		struct ia_css_blob_descr bd;
		enum ia_css_err err;

		err = sh_css_load_blob_info(fw_data, bi, &bd, i);

		if (err != IA_CSS_SUCCESS)
			return IA_CSS_ERR_INTERNAL_ERROR;

		if (bi->blob.offset + bi->blob.size > fw_size)
			return IA_CSS_ERR_INTERNAL_ERROR;

		if (bi->type == ia_css_sp_firmware) {
			if (i != SP_FIRMWARE)
				return IA_CSS_ERR_INTERNAL_ERROR;
			err = setup_sp(bi, fw_data, &sh_css_sp_fw, i);
			if (err != IA_CSS_SUCCESS)
				return err;
#if defined(ENABLE_SP1)
		} else if (bi->type == ia_css_sp1_firmware) {
			if (i != SP1_FIRMWARE)
				return IA_CSS_ERR_INTERNAL_ERROR;
			err = setup_sp(bi, fw_data, &sh_css_sp1_fw, i);
			if (err != IA_CSS_SUCCESS)
				return err;
#endif
		} else {
			/* All subsequent binaries (i>NUM_OF_SPS) are ISP firmware */
			if (i < NUM_OF_SPS)
				return IA_CSS_ERR_INTERNAL_ERROR;
			if (bi->type != ia_css_isp_firmware)
				return IA_CSS_ERR_INTERNAL_ERROR;
			if (sh_css_blob_info == NULL) /* cannot happen but KW does not see this */
				return IA_CSS_ERR_INTERNAL_ERROR;
			sh_css_blob_info[i-NUM_OF_SPS] = bd;
		}
	}

	return IA_CSS_SUCCESS;
}

void sh_css_unload_firmware(void)
{

	/* release firmware minibuffer */
	if (fw_minibuffer) {
		unsigned int i = 0;
		for (i = 0; i < sh_css_num_binaries; i++) {
			if (fw_minibuffer[i].name)
				sh_css_free((void *)fw_minibuffer[i].name);
			if (fw_minibuffer[i].buffer)
				sh_css_free((void *)fw_minibuffer[i].buffer);
		}
		sh_css_free(fw_minibuffer);
		fw_minibuffer = NULL;
	}

	memset(&sh_css_sp_fw, 0, sizeof(sh_css_sp_fw));
#if defined(ENABLE_SP1)
	memset(&sh_css_sp1_fw, 0, sizeof(sh_css_sp1_fw));
#endif
	if (sh_css_blob_info) {
		sh_css_free(sh_css_blob_info);
		sh_css_blob_info = NULL;
	}
	sh_css_num_binaries = 0;
}

hrt_vaddress
sh_css_load_blob(const unsigned char *blob, unsigned size)
{
	hrt_vaddress target_addr = mmgr_malloc(size);
	/* this will allocate memory aligned to a DDR word boundary which
	   is required for the CSS DMA to read the instructions. */

	assert(blob != NULL);
	if (target_addr) {
		mmgr_store(target_addr, blob, size);
#ifdef HRT_CSIM
		{
			unsigned padded_size = CEIL_MUL(size, HIVE_ISP_DDR_WORD_BYTES);
			mmgr_clear(target_addr + size, padded_size - size);
		}
#endif
	}
	return target_addr;
}