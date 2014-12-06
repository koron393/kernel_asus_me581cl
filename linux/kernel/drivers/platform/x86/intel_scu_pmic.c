/*
 * pmic.c - Intel MSIC Driver
 *
 * Copyright (C) 2012 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Bin Yang <bin.yang@intel.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/rpmsg.h>
#include <asm/intel_scu_pmic.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_mid_rpmsg.h>
#include <linux/platform_data/intel_mid_remoteproc.h>
//=================stree test====================
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
//=================stree test end =================

#define IPC_WWBUF_SIZE    20
#define IPC_RWBUF_SIZE    20

static struct kobject *scu_pmic_kobj;
static struct rpmsg_instance *pmic_instance;

#define PMIC_REG_DEF(x) { .reg_name = #x, .addr = x }
#define PMICSPARE01_ADDR		0x109
#define LOWBATTDET1_ADDR		0x2D
#define BATTDETCTRL_ADDR		0x2E
#define PBCONFIG_ADDR			0x29
#define VDD3CNT_ADDR			0xAA
#define FLTCFG_ADDR				0x18

struct pmic_regs_define {
	char reg_name[28];
	u16 addr;
};

static struct pmic_regs_define pmic_regs_sc[] = {
	PMIC_REG_DEF(PMICSPARE01_ADDR	), //don't change location.
	PMIC_REG_DEF(LOWBATTDET1_ADDR),
	PMIC_REG_DEF(BATTDETCTRL_ADDR),
	PMIC_REG_DEF(PBCONFIG_ADDR),
	PMIC_REG_DEF(VDD3CNT_ADDR	),
	PMIC_REG_DEF(FLTCFG_ADDR),
};
//=================stree test=================

struct shady_cove_stress {
	struct delayed_work stress_test;
	struct miscdevice shady_miscdev;
};
struct workqueue_struct *shady_strees_work_queue = NULL;

static struct shady_cove_stress *pshady_cove_stress;
static int pmic_status = 0;

#define SHADY_IOC_MAGIC		0xFB
#define SHADY_IOC_MAXNR	5
#define SHADY_POLLING_DATA _IOR(SHADY_IOC_MAGIC, 1, int)
#define TEST_END	(0)
#define START_NORMAL 	(1)
#define START_HEAVY	(2)
#define IOCTL_ERROR 	(-1)

#define PMIC_VENDOR_ID_MASK	(0x03 << 6)
#define PMIC_ID_ADDR			0x00
#define SHADYCOVE_VENDORID	0x00

static ssize_t pmic_show_status(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", pmic_status);
}
static KOBJ_PMIC_ATTR(pmic_status,  S_IRUGO|S_IWUSR, pmic_show_status, NULL);

extern void kernel_power_off(void);
static ssize_t debug_shutdown_store(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf, size_t size)
{
	u32 off;
	int ret;

	ret = sscanf(buf, "%x", &off);
	if (ret == 0) {
		printk(KERN_EMERG "error: shutdown device  by debugging funnction! %d\n", ret);
		return size;
	}

	printk(KERN_EMERG "device shutdown by debugging! %u\n", off);
	if (off == 1) {
		emergency_sync();
		kernel_power_off();
	}

	return size;
}
static KOBJ_PMIC_ATTR(debug_shutdown,  S_IRUGO|S_IWUSR, NULL, debug_shutdown_store);

static ssize_t pmic_nvm_version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	u8 data;

	retval = intel_scu_ipc_ioread8(PMICSPARE01_ADDR, &data);
	if (!retval)
		printk(KERN_INFO "========pmic_nvm_version_show is 0x%x============\n", data);
	return snprintf(buf, PAGE_SIZE, "%#x\n", data);
}
static KOBJ_PMIC_ATTR(nvm_version,  S_IRUGO|S_IWUSR, pmic_nvm_version_show, NULL);

//=================stree test end=================

static int pwr_reg_rdwr(u16 *addr, u8 *data, u32 count, u32 cmd, u32 sub)
{
	int i, err, inlen = 0, outlen = 0;

	u8 wbuf[IPC_WWBUF_SIZE] = {};
	u8 rbuf[IPC_RWBUF_SIZE] = {};

	memset(wbuf, 0, sizeof(wbuf));

	for (i = 0; i < count; i++) {
		wbuf[inlen++] = addr[i] & 0xff;
		wbuf[inlen++] = (addr[i] >> 8) & 0xff;
	}

	if (sub == IPC_CMD_PCNTRL_R) {
		outlen = count > 0 ? ((count - 1) / 4) + 1 : 0;
	} else if (sub == IPC_CMD_PCNTRL_W) {
		if (count == 3)
			inlen += 2;

		for (i = 0; i < count; i++)
			wbuf[inlen++] = data[i] & 0xff;

		if (count == 3)
			inlen -= 2;

		outlen = 0;
	} else if (sub == IPC_CMD_PCNTRL_M) {
		wbuf[inlen++] = data[0] & 0xff;
		wbuf[inlen++] = data[1] & 0xff;
		outlen = 0;
	} else
		pr_err("IPC command not supported\n");

	err = rpmsg_send_command(pmic_instance, cmd, sub, wbuf,
			(u32 *)rbuf, inlen, outlen);

	if (sub == IPC_CMD_PCNTRL_R) {
		for (i = 0; i < count; i++)
			data[i] = rbuf[i];
	}

	return err;
}

int intel_scu_ipc_ioread8(u16 addr, u8 *data)
{
	return pwr_reg_rdwr(&addr, data, 1, IPCMSG_PCNTRL, IPC_CMD_PCNTRL_R);
}
EXPORT_SYMBOL(intel_scu_ipc_ioread8);

int intel_scu_ipc_iowrite8(u16 addr, u8 data)
{
	return pwr_reg_rdwr(&addr, &data, 1, IPCMSG_PCNTRL, IPC_CMD_PCNTRL_W);
}
EXPORT_SYMBOL(intel_scu_ipc_iowrite8);

int intel_scu_ipc_iowrite32(u16 addr, u32 data)
{
	u16 x[4] = {addr, addr + 1, addr + 2, addr + 3};
	return pwr_reg_rdwr(x, (u8 *)&data, 4, IPCMSG_PCNTRL, IPC_CMD_PCNTRL_W);
}
EXPORT_SYMBOL(intel_scu_ipc_iowrite32);

int intel_scu_ipc_readv(u16 *addr, u8 *data, int len)
{
	if (len < 1 || len > 8)
		return -EINVAL;

	return pwr_reg_rdwr(addr, data, len, IPCMSG_PCNTRL, IPC_CMD_PCNTRL_R);
}
EXPORT_SYMBOL(intel_scu_ipc_readv);

int intel_scu_ipc_writev(u16 *addr, u8 *data, int len)
{
	if (len < 1 || len > 4)
		return -EINVAL;

	return pwr_reg_rdwr(addr, data, len, IPCMSG_PCNTRL, IPC_CMD_PCNTRL_W);
}
EXPORT_SYMBOL(intel_scu_ipc_writev);

int intel_scu_ipc_update_register(u16 addr, u8 bits, u8 mask)
{
	u8 data[2] = { bits, mask };
	return pwr_reg_rdwr(&addr, data, 1, IPCMSG_PCNTRL, IPC_CMD_PCNTRL_M);
}
EXPORT_SYMBOL(intel_scu_ipc_update_register);

/* pmic sysfs for debug */

#define MAX_PMIC_REG_NR 4
#define PMIC_OPS_LEN 10

enum {
	PMIC_DBG_ADDR,
	PMIC_DBG_BITS,
	PMIC_DBG_DATA,
	PMIC_DBG_MASK,
};

static char *pmic_msg_format[] = {
	"addr[%d]: %#x\n",
	"bits[%d]: %#x\n",
	"data[%d]: %#x\n",
	"mask[%d]: %#x\n",
};

static u16 pmic_reg_addr[MAX_PMIC_REG_NR];
static u8 pmic_reg_bits[MAX_PMIC_REG_NR];
static u8 pmic_reg_data[MAX_PMIC_REG_NR];
static u8 pmic_reg_mask[MAX_PMIC_REG_NR];
static int valid_addr_nr;
static int valid_bits_nr;
static int valid_data_nr;
static int valid_mask_nr;
static char pmic_ops[PMIC_OPS_LEN];

static int pmic_dbg_error;

static ssize_t pmic_generic_show(char *buf, int valid, u8 *array, int type)
{
	int i, buf_size;
	ssize_t ret = 0;

	switch (type) {
	case PMIC_DBG_ADDR:
		for (i = 0; i < valid; i++) {
			buf_size = PAGE_SIZE - ret;
			ret += snprintf(buf + ret, buf_size,
					pmic_msg_format[type],
					i, pmic_reg_addr[i]);
		}
		break;
	case PMIC_DBG_BITS:
	case PMIC_DBG_DATA:
	case PMIC_DBG_MASK:
		for (i = 0; i < valid; i++) {
			buf_size = PAGE_SIZE - ret;
			ret += snprintf(buf + ret, buf_size,
					pmic_msg_format[type],
					i, array[i]);
		}
		break;
	default:
		break;
	}

	return ret;
}

static void pmic_generic_store(const char *buf, int *valid, u8 *array, int type)
{
	u32 tmp[MAX_PMIC_REG_NR];
	int i, ret;

	ret = sscanf(buf, "%x %x %x %x", &tmp[0], &tmp[1], &tmp[2], &tmp[3]);
	if (ret == 0 || ret > MAX_PMIC_REG_NR) {
		*valid = 0;
		pmic_dbg_error = -EINVAL;
		return;
	}

	*valid = ret;

	switch (type) {
	case PMIC_DBG_ADDR:
		memset(pmic_reg_addr, 0, sizeof(pmic_reg_addr));
		for (i = 0; i < ret; i++)
			pmic_reg_addr[i] = (u16)tmp[i];
		break;
	case PMIC_DBG_BITS:
	case PMIC_DBG_DATA:
	case PMIC_DBG_MASK:
		memset(array, 0, sizeof(*array) * MAX_PMIC_REG_NR);
		for (i = 0; i < ret; i++)
			array[i] = (u8)tmp[i];
		break;
	default:
		break;
	}
}

static ssize_t pmic_addr_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return pmic_generic_show(buf, valid_addr_nr, NULL, PMIC_DBG_ADDR);
}

static ssize_t pmic_addr_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t size)
{
	pmic_generic_store(buf, &valid_addr_nr, NULL, PMIC_DBG_ADDR);
	return size;
}

static ssize_t pmic_bits_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return pmic_generic_show(buf, valid_bits_nr, pmic_reg_bits,
			PMIC_DBG_BITS);
}
static ssize_t pmic_bits_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t size)
{
	pmic_generic_store(buf, &valid_bits_nr, pmic_reg_bits, PMIC_DBG_BITS);
	return size;
}

static ssize_t pmic_data_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return pmic_generic_show(buf, valid_data_nr, pmic_reg_data,
			PMIC_DBG_DATA);
}

static ssize_t pmic_data_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t size)
{
	pmic_generic_store(buf, &valid_data_nr, pmic_reg_data, PMIC_DBG_DATA);
	return size;
}

static ssize_t pmic_mask_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return pmic_generic_show(buf, valid_mask_nr, pmic_reg_mask,
			PMIC_DBG_MASK);
}

static ssize_t pmic_mask_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t size)
{
	pmic_generic_store(buf, &valid_mask_nr, pmic_reg_mask, PMIC_DBG_MASK);
	return size;
}

static ssize_t pmic_ops_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t size)
{
	int i, ret;

	memset(pmic_ops, 0, sizeof(pmic_ops));

	ret = sscanf(buf, "%9s", pmic_ops);
	if (ret == 0) {
		pmic_dbg_error = -EINVAL;
		goto end;
	}

	if (valid_addr_nr <= 0) {
		pmic_dbg_error = -EINVAL;
		goto end;
	}

	if (!strncmp("read", pmic_ops, PMIC_OPS_LEN)) {
		valid_data_nr = valid_addr_nr;
		for (i = 0; i < valid_addr_nr; i++) {
			ret = intel_scu_ipc_ioread8(pmic_reg_addr[i],
					&pmic_reg_data[i]);
			if (ret) {
				pmic_dbg_error = ret;
				goto end;
			}
		}
	} else if (!strncmp("write", pmic_ops, PMIC_OPS_LEN)) {
		if (valid_addr_nr == valid_data_nr) {
			for (i = 0; i < valid_addr_nr; i++) {
				ret = intel_scu_ipc_iowrite8(pmic_reg_addr[i],
						pmic_reg_data[i]);
				if (ret) {
					pmic_dbg_error = ret;
					goto end;
				}
			}
		} else {
			pmic_dbg_error = -EINVAL;
			goto end;
		}
	} else if (!strncmp("update", pmic_ops, PMIC_OPS_LEN)) {
		if (valid_addr_nr == valid_mask_nr &&
				valid_mask_nr == valid_bits_nr) {
			for (i = 0; i < valid_addr_nr; i++) {
				ret = intel_scu_ipc_update_register(
						pmic_reg_addr[i],
						pmic_reg_bits[i],
						pmic_reg_mask[i]);
				if (ret) {
					pmic_dbg_error = ret;
					goto end;
				}
			}
		} else {
			pmic_dbg_error = -EINVAL;
			goto end;
		}
	} else {
		pmic_dbg_error = -EINVAL;
		goto end;
	}

		pmic_dbg_error = 0;

end:
	return size;
}

static ssize_t pmic_show_error(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", pmic_dbg_error);
}

static KOBJ_PMIC_ATTR(addr, S_IRUSR|S_IWUSR, pmic_addr_show, pmic_addr_store);
static KOBJ_PMIC_ATTR(bits, S_IRUSR|S_IWUSR, pmic_bits_show, pmic_bits_store);
static KOBJ_PMIC_ATTR(data, S_IRUSR|S_IWUSR, pmic_data_show, pmic_data_store);
static KOBJ_PMIC_ATTR(mask, S_IRUSR|S_IWUSR, pmic_mask_show, pmic_mask_store);
static KOBJ_PMIC_ATTR(ops, S_IWUSR, NULL, pmic_ops_store);
static KOBJ_PMIC_ATTR(error, S_IRUSR, pmic_show_error, NULL);

static struct attribute *pmic_attrs[] = {
	&addr_attr.attr,
	&bits_attr.attr,
	&data_attr.attr,
	&mask_attr.attr,
	&ops_attr.attr,
	&error_attr.attr,
	//=================stree test=================
	&pmic_status_attr.attr,
	&debug_shutdown_attr.attr,
	&nvm_version_attr.attr,
	//=================stree test end=================
	NULL,
};

static struct attribute_group pmic_attr_group = {
	.name = "pmic_debug",
	.attrs = pmic_attrs,
};

//=================stree test=================
void shady_read_stress_test(struct work_struct *work)
{
	u8 pmic_id;
	int ret;

	ret = intel_scu_ipc_ioread8(PMIC_ID_ADDR, &pmic_id);
	if (ret)
		printk(KERN_ERR "error: shady_read_stress_test: can NOT read PMIC ID\n");
	else if ((pmic_id & PMIC_VENDOR_ID_MASK) != SHADYCOVE_VENDORID)
		printk(KERN_ERR "error: shady_read_stress_tes: pmic_id != SHADYCOVE_VENDORID, id = %x\n", pmic_id & PMIC_VENDOR_ID_MASK);
	else {
		printk(KERN_INFO "shady_read_stress_test: pmic_id = %x\n", pmic_id & PMIC_VENDOR_ID_MASK);
	}

	queue_delayed_work( shady_strees_work_queue, &pshady_cove_stress->stress_test, 2*HZ);
	return;
}
long   shady_ioctl(struct file *filp,  unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) == SHADY_IOC_MAGIC)
	{
	     printk(" shady_ioctl vaild magic \n");
	}
	else
	{
		printk(" shady_ioctl invaild magic \n");
		return -ENOTTY;
	}

	switch(cmd)
	{
		case SHADY_POLLING_DATA :
		if ((arg == START_NORMAL) ||(arg == START_HEAVY))
		{
				 printk(" shady stress test start (%s)\n", (arg==START_NORMAL) ? "normal" : "heavy");
				 queue_delayed_work(shady_strees_work_queue, &pshady_cove_stress->stress_test, 2*HZ);
		}
		else
		{
				 printk("shady tress test end\n");
				 cancel_delayed_work_sync(&pshady_cove_stress->stress_test);
		}
		break;

		default:  /* redundant, as cmd was checked against MAXNR */
			printk(" shady: unknow i2c  stress test  command cmd=%x arg=%lu\n", cmd, arg);
			return -ENOTTY;
	}

	return 0;
}

int shady_open(struct inode *inode, struct file *filp)
{
	return 0;
}

struct file_operations shady_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = shady_ioctl,
	#ifdef CONFIG_COMPAT
	.compat_ioctl   = shady_ioctl,
	#endif
	.open =  shady_open,
};

static void dump_pmic_regs(void)
{
	u32 pmic_reg_cnt = 0;
	u32 reg_index;
	u8 data;
	int retval;
	struct pmic_regs_define *pmic_regs = NULL;

	pmic_reg_cnt = ARRAY_SIZE(pmic_regs_sc);
	pmic_regs = pmic_regs_sc;

	printk(KERN_INFO "intel_scu_pmic PMIC Register dump\n");

	retval = intel_scu_ipc_ioread8(pmic_regs[0].addr, &data);
	if (!retval)
		printk(KERN_INFO "========NVM version is 0x%x============\n", data);

	for (reg_index = 0; reg_index < pmic_reg_cnt; reg_index++) {

		retval = intel_scu_ipc_ioread8(pmic_regs[reg_index].addr,
				&data);
		if (retval)
			printk(KERN_ERR "Error in reading %x\n",
				pmic_regs[reg_index].addr);
		else
			printk(KERN_INFO "0x%x=0x%x\n",
				pmic_regs[reg_index].addr, data);
	}
	printk(KERN_INFO "====================\n");
}

//=================stree test end=================

static int pmic_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;
	//=================stree test ===================
	u8 pmic_id;
	int rc;
	//=================stree test end=================

	if (rpdev == NULL) {
		pr_err("rpmsg channel not created\n");
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed pmic rpmsg device\n");

	/* Allocate rpmsg instance for pmic*/
	ret = alloc_rpmsg_instance(rpdev, &pmic_instance);
	if (!pmic_instance) {
		dev_err(&rpdev->dev, "kzalloc pmic instance failed\n");
		goto out;
	}
	/* Initialize rpmsg instance */
	init_rpmsg_instance(pmic_instance);

	/* Create debugfs for pmic regs */
	scu_pmic_kobj = kobject_create_and_add(pmic_attr_group.name,
						kernel_kobj);

	ret = intel_scu_ipc_ioread8(PMIC_ID_ADDR, &pmic_id);
	if (ret)
		printk(KERN_ERR "error: pmic_rpmsg_probe: can NOT read PMIC ID");
	else if ((pmic_id & PMIC_VENDOR_ID_MASK) != SHADYCOVE_VENDORID)
		printk(KERN_ERR "error: pmic_rpmsg_probe: pmic_id != SHADYCOVE_VENDORID, id = %x", pmic_id & PMIC_VENDOR_ID_MASK);
	else {
		printk(KERN_INFO "pmic_rpmsg_probe: pmic_id = %x", pmic_id & PMIC_VENDOR_ID_MASK);
		pmic_status = 1;
		dump_pmic_regs();
	}

	if (!scu_pmic_kobj) {
		ret = -ENOMEM;
		goto rpmsg_err;
	}

	ret = sysfs_create_group(scu_pmic_kobj, &pmic_attr_group);

	if (ret) {
		kobject_put(scu_pmic_kobj);
		goto rpmsg_err;
	}

	//=================stree test=============================================
	pshady_cove_stress = devm_kzalloc(&rpdev->dev,
						sizeof(struct shady_cove_stress), GFP_KERNEL);
	if (!pshady_cove_stress) {
		sysfs_remove_group(scu_pmic_kobj, &pmic_attr_group);
		kobject_put(scu_pmic_kobj);
		ret = -ENOMEM;
		goto rpmsg_err;
	}
       INIT_DELAYED_WORK(&pshady_cove_stress->stress_test, shady_read_stress_test) ;
       shady_strees_work_queue = create_singlethread_workqueue("shady_strees_test_workqueue");
	pshady_cove_stress->shady_miscdev.minor	= MISC_DYNAMIC_MINOR;
	pshady_cove_stress->shady_miscdev.name	= "shady_stress_test";
	pshady_cove_stress->shady_miscdev.fops  	= &shady_fops;
       rc = misc_register(&pshady_cove_stress->shady_miscdev);
	printk(KERN_INFO "pmic_rpmsg_probe: shady cove register misc device for I2C stress test rc=%x\n", rc);
	//=================stree test end============================================

	goto out;

rpmsg_err:
	free_rpmsg_instance(rpdev, &pmic_instance);
out:
	return ret;
}

static void pmic_rpmsg_remove(struct rpmsg_channel *rpdev)
{
	free_rpmsg_instance(rpdev, &pmic_instance);
	sysfs_remove_group(scu_pmic_kobj, &pmic_attr_group);
	kobject_put(scu_pmic_kobj);
	dev_info(&rpdev->dev, "Removed pmic rpmsg device\n");
}

static void pmic_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
}

static struct rpmsg_device_id pmic_rpmsg_id_table[] = {
	{ .name	= "rpmsg_pmic" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, pmic_rpmsg_id_table);

static struct rpmsg_driver pmic_rpmsg = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= pmic_rpmsg_id_table,
	.probe		= pmic_rpmsg_probe,
	.callback	= pmic_rpmsg_cb,
	.remove		= pmic_rpmsg_remove,
};

static int __init pmic_rpmsg_init(void)
{
	return register_rpmsg_driver(&pmic_rpmsg);
}

#ifdef MODULE
module_init(pmic_rpmsg_init);
#else
fs_initcall_sync(pmic_rpmsg_init);
#endif

static void __exit pmic_rpmsg_exit(void)
{
	return unregister_rpmsg_driver(&pmic_rpmsg);
}
module_exit(pmic_rpmsg_exit);

MODULE_AUTHOR("Bin Yang<bin.yang@intel.com>");
MODULE_DESCRIPTION("Intel PMIC Driver");
MODULE_LICENSE("GPL v2");
