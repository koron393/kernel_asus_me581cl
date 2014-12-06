/*
 * Intel NPK trace driver provides ioctl to access NPK trace buffer
 *
 * Copyright (c) 2014 Intel Corporation, Kai Li <kai.li@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/npk.h>
#include "npk_pci.h"
#include "npk_trace.h"

/* MSU stands for Memory Storage Unit
 * In the North Peak hardware, the MSU block is
 * responsible for routing trace data to system memory.
 */
static struct msu_ctx msu[MAX_NUM_MSU];
static struct msu_ctx *msu_csr;
static struct msu_ctx *msu_win;

/* This function assumes the caller has grabbed the mutex
 * that protects the npk device
 */
static int chain_blks(struct npk_pci_device *npk_dev, u32 idx)
{
	int i, j;
	struct msu_window *win;
	struct msu_blk_entry *entry;
	struct npk_msu_reg *msu_reg = &npk_dev->dev_info->msu_reg[idx];

	for (i = 0; i < msu[idx].nr_of_wins; i++) {
		dma_addr_t next_win_pa;
		u32 sw_tag = 0;

		if (i == msu[idx].nr_of_wins - 1) {
			next_win_pa = msu[idx].win_list[0].blk_list[0].blk_pa;
			sw_tag = SW_TAG_LAST_WIN;
		} else
			next_win_pa = msu[idx].win_list[i+1].blk_list[0].blk_pa;

		win = &msu[idx].win_list[i];
		for (j = 0; j < win->nr_of_blks; j++) {
			dma_addr_t next_blk_pa;

			if (j == win->nr_of_blks - 1) {
				next_blk_pa = win->blk_list[0].blk_pa;
				sw_tag |= SW_TAG_LAST_BLK;
			} else
				next_blk_pa = win->blk_list[j+1].blk_pa;

			entry = (struct msu_blk_entry *)win->blk_list[j].blk;

			entry->sw_tag = sw_tag;
			entry->blk_size = BLK_ENTRIES_PER_PAGE;
			entry->next_blk_addr = next_blk_pa >> PAGE_SHIFT;
			entry->next_win_addr = next_win_pa >> PAGE_SHIFT;
		}
	}

	msu[idx].mem = msu[idx].win_list[0].blk_list[0].blk;
	msu[idx].mem_pa = msu[idx].win_list[0].blk_list[0].blk_pa;

	if (npk_reg_write_no_lock(msu_reg->mscbar, msu[idx].mem_pa >> PAGE_SHIFT))
		return -EFAULT;

	return 0;
}

/* This function assumes the caller has grabbed the mutex
 * that protects the npk device
 */
static void free_blks(struct npk_pci_device *npk_dev,
		      struct msu_window *win)
{
	int i = 0;
	struct msu_block *block;
	struct device *dev = &npk_dev->pdev->dev;

	if (!win || !win->blk_list)
		return;

	for (i = 0; i < win->nr_of_blks; i++) {
		block = &win->blk_list[i];

		if (!block->blk)
			break;

		dma_free_coherent(dev, PAGE_SIZE, block->blk, block->blk_pa);
		win->blk_list[i].blk = NULL;
	}

	kfree(win->blk_list);
	win->blk_list = NULL;
}

/* This function assumes the caller has grabbed the mutex
 * that protects the npk device
 */
static int
alloc_blks(struct npk_pci_device *npk_dev,
	   struct msu_window *win,
	   unsigned long nr_of_blks)
{
	int i;
	struct msu_block block;
	struct device *dev = &npk_dev->pdev->dev;

	win->nr_of_blks = nr_of_blks;
	win->blk_list = kzalloc(nr_of_blks * sizeof(struct msu_block),
				GFP_KERNEL);
	if (!win->blk_list) {
		pr_err("no mem for blk list\n");
		return -ENOMEM;
	}

	for (i = 0; i < win->nr_of_blks; i++) {
		block.blk = dma_alloc_coherent(dev, PAGE_SIZE, &block.blk_pa,
					       GFP_KERNEL);
		if (!block.blk) {
			free_blks(npk_dev, win);
			return -ENOMEM;
		}

		win->blk_list[i] = block;
	}

	return 0;
}

/* This function assumes the caller has grabbed the mutex
 * that protects the npk device
 */
static int free_winmem(struct npk_pci_device *npk_dev, u32 idx)
{
	int i;
	struct npk_msu_reg *msu_reg;

	if (idx >= ARRAY_SIZE(msu) || msu[idx].win_list == NULL
	    || msu[idx].nr_of_wins == 0) {
		return -EFAULT;
	}

	for (i = 0; i < msu[idx].nr_of_wins; i++) {
		struct msu_window *win;
		win = &msu[idx].win_list[i];
		free_blks(npk_dev, win);
	}

	kfree(msu[idx].win_list);

	memset(&msu[idx], 0, sizeof(struct msu_ctx));

	msu_reg = &npk_dev->dev_info->msu_reg[idx];
	if (npk_reg_write_no_lock(msu_reg->mscbar, 0))
		return -EFAULT;

	return 0;
}

/* This function assumes the caller has grabbed the mutex
 * that protects the msu context
 */
static int alloc_winmem(struct npk_pci_device *npk_dev,
			struct npk_win_info *info)
{
	int i;
	int ret;
	unsigned long nr_of_blks;
	u32 idx, nr_of_wins, win_size;

	idx = info->idx;
	nr_of_wins = info->num_windows;
	win_size = info->window_size;

	if (nr_of_wins == 0 || nr_of_wins > MAX_NUM_WINDOWS ||
	    win_size == 0 || win_size > MAX_WINDOW_SIZE ||
	    idx >= ARRAY_SIZE(msu)) {
		pr_err("invalid window parameters\n");
		return -EINVAL;
	}

	if (msu[idx].used) {
		pr_err("msu already in use\n");
		return -EINVAL;
	}

	msu[idx].win_list = kzalloc(nr_of_wins * sizeof(struct msu_window),
				    GFP_KERNEL);
	if (!msu[idx].win_list) {
		pr_err("no mem for win list\n");
		return -ENOMEM;
	}

	nr_of_blks = DIV_ROUND_UP(win_size, PAGE_SIZE);

	for (i = 0; i < nr_of_wins; i++) {
		ret = alloc_blks(npk_dev, &msu[idx].win_list[i], nr_of_blks);
		if (ret) {
			free_winmem(npk_dev, idx);
			return ret;
		}
	}

	msu[idx].nr_of_wins = nr_of_wins;
	msu[idx].win_size = win_size;
	msu[idx].mode = MSU_MODE_WIN;
	msu[idx].used = true;
	msu[idx].idx = idx;
	msu[idx].size = nr_of_wins * nr_of_blks * PAGE_SIZE;

	info->num_blks = nr_of_blks;
	info->blk_size = PAGE_SIZE;

	return 0;
}

/**
 * For debug purpose, this function prints information
 * on the trace buffer associated to a given msu
 *
 * This function assumes the caller has grabbed the mutex
 * that protects the npk device
 */
static int print_msu_info(unsigned long arg)
{
	int i, j;
	u32 idx;
	struct msu_window *win;
	struct msu_blk_entry *entry;

	if (get_user(idx, (u32 __user *)arg)) {
		pr_err("error while retrieving msu id\n");
		return -EFAULT;
	}

	if (idx >= ARRAY_SIZE(msu)) {
		pr_err("invalid window parameters\n");
		return -EINVAL;
	}

	pr_info("base addr 0x%lx\n", (unsigned long)msu[idx].mem_pa >> PAGE_SHIFT);
	pr_info("size 0x%x\n", msu[idx].size);
	pr_info("mode %d\n", msu[idx].mode);

	if (msu[idx].mode == MSU_MODE_WIN) {

		for (i = 0; i < msu[idx].nr_of_wins; i++) {
			win = &msu[idx].win_list[i];

			pr_info("\t win index: %d at 0x%p\n", i, win);

			for (j = 0; j < win->nr_of_blks; j++) {
				entry = (struct msu_blk_entry *)win->blk_list[j].blk;

				pr_info("\t blk index: %d at 0x%p\n", j, entry);
				pr_info("\t sw tag: %d\n", entry->sw_tag);
				pr_info("\t blk size: %d\n", entry->blk_size);
				pr_info("\t next blk: 0x%x\n", entry->next_blk_addr);
				pr_info("\t next win: 0x%x\n", entry->next_win_addr);
			}
		}
	}

	return 0;
}

/**
 * Allocation of a linked-list mode buffer for NPK trace
 *
 * This function assumes the caller has grabbed the mutex
 * that protects the npk device
 */
static int alloc_window(struct npk_pci_device *npk_dev, unsigned long arg)
{
	struct npk_win_info info;
	int ret;

	if (copy_from_user(&info, (void __user *)arg,
			   sizeof(struct npk_win_info)))
		return -EFAULT;

	ret = alloc_winmem(npk_dev, &info);
	if (ret)
		return ret;

	ret = chain_blks(npk_dev, info.idx);
	if (ret) {
		free_winmem(npk_dev, info.idx);
		return ret;
	}

	if (copy_to_user((void __user *)arg, &info,
			  sizeof(struct npk_win_info))) {
		free_winmem(npk_dev, info.idx);
		return -EFAULT;
	}

	msu_win = &msu[info.idx];

	return 0;
}

/**
 * Release of a linked-list mode buffer for NPK trace
 *
 * This function assumes the caller has grabbed the mutex
 * that protects the npk device
 */
static int free_window(struct npk_pci_device *npk_dev, unsigned long arg)
{
	u32 idx;

	if (get_user(idx, (u32 __user *)arg))
		return -EFAULT;

	if (free_winmem(npk_dev, idx))
		return -EFAULT;

	msu_win = NULL;

	return 0;
}

/* This function allocates a CSR-driven mode buffer for NPK trace.
 * The allocated memory is contiguous in physical memory.
 * The MSCnDESTSZ and MSCnBAR registers are updated with the address
 * and size of the buffer.
 */
int npk_alloc_kmem(struct npk_pci_device *npk_dev, u32 idx, u32 size)
{
	struct npk_msu_reg *msu_reg = &npk_dev->dev_info->msu_reg[idx];
	struct device *dev = &npk_dev->pdev->dev;

	if (size == 0 ||
	    size > MAX_SINGLE_BLOCK_BUFFER_SIZE ||
	    idx >= ARRAY_SIZE(msu))
		return -EINVAL;

	if (msu[idx].used) {
		if (msu[idx].mode != MSU_MODE_CSR)
			return -EINVAL;
		if (msu[idx].size >= size) {
			return 0;
		} else {
			dma_free_coherent(dev, size, msu[idx].mem, msu[idx].mem_pa);
			memset(&msu[idx], 0, sizeof(struct msu_ctx));
		}
	}

	msu[idx].mem = dma_alloc_coherent(dev, size, &msu[idx].mem_pa,
					  GFP_KERNEL);

	if (!msu[idx].mem)
		return -ENOMEM;

	msu[idx].size = size;
	msu[idx].used = true;
	msu[idx].mode = MSU_MODE_CSR;
	msu[idx].readpos = 0;
	msu[idx].len = 0;
	msu[idx].idx = idx;
	msu[idx].wrapstat = 0;

	if ((npk_reg_write_no_lock(msu_reg->mscdestsz, size >> PAGE_SHIFT)) ||
	    (npk_reg_write_no_lock(msu_reg->mscbar, msu[idx].mem_pa >> PAGE_SHIFT))) {
		pr_err("register write failed\n");
		dma_free_coherent(dev, size, msu[idx].mem, msu[idx].mem_pa);
		memset(&msu[idx], 0, sizeof(struct msu_ctx));
		return -EFAULT;
	}

	return 0;
}

/* This function releases a CSR-driven mode buffer.
 * The MSCnDESTSZ and MSCnBAR registers are reset.
 */
int npk_free_kmem(struct npk_pci_device *npk_dev, u32 idx)
{
	struct npk_msu_reg *msu_reg = &npk_dev->dev_info->msu_reg[idx];
	struct device *dev = &npk_dev->pdev->dev;

	if (idx >= ARRAY_SIZE(msu) || !msu[idx].mem
	    || msu[idx].size == 0 || msu[idx].mode != MSU_MODE_CSR)
		return -EFAULT;

	dma_free_coherent(dev, msu[idx].size, msu[idx].mem, msu[idx].mem_pa);
	memset(&msu[idx], 0, sizeof(struct msu_ctx));

	if ((npk_reg_write_no_lock(msu_reg->mscdestsz, 0)) ||
	    (npk_reg_write_no_lock(msu_reg->mscbar, 0))) {
		pr_err("register write failed\n");
		return -EFAULT;
	}

	return 0;
}

/**
 * Allocation of a CSR-driven mode buffer for NPK trace
 *
 * This function assumes the caller has grabbed the mutex
 * that protects the npk device
 */
static int alloc_buffer(struct npk_pci_device *npk_dev,
			unsigned long arg)
{
	struct npk_csr_info info;

	if (copy_from_user(&info, (void __user *)arg,
			   sizeof(struct npk_csr_info)))
		return -EFAULT;

	if (npk_alloc_kmem(npk_dev, info.idx, info.size))
		return -EFAULT;

	msu_csr = &msu[info.idx];

	return 0;

}

/**
 * Release of a CSR-driven mode buffer for NPK trace
 *
 * This function assumes the caller has grabbed the mutex
 * that protects the npk device
 */
static int free_buffer(struct npk_pci_device *npk_dev,
		       unsigned long arg)
{
	u32  idx;

	if (get_user(idx, (u32 __user *)arg))
		return -EFAULT;

	if (npk_free_kmem(npk_dev, idx))
		return -EFAULT;

	msu_csr = NULL;

	return 0;
}

/**
 * This function reads the CSR-driven mode buffer.
 * Depending on whether there has been a wrap in the
 * buffer, one or two copy operations may be necessary.
 *
 * This function assumes the caller has grabbed the mutex
 * that protects the npk device
 */
static ssize_t trace_data_to_user(struct npk_pci_device *npk_dev,
				  struct msu_ctx *msu,
				  char __user *ubuff, size_t cnt)
{
	u32 cmwp, len;
	void *start;
	u8 *msu_mem;
	struct npk_msu_reg *msu_reg;

	msu_reg = &npk_dev->dev_info->msu_reg[msu->idx];

	if (msu->len == 0) {
		u32 num_bytes, bar, sts;

		/* MSCnBAR stores the base address of the buffer
		 * MSCnMWP stores the write pointer address
		 * WRAPSTAT (in MSCnSTS) tells whether the buffer has wrapped
		 */
		if ((npk_reg_read_no_lock(msu_reg->mscmwp, &cmwp)) ||
		    (npk_reg_read_no_lock(msu_reg->mscbar, &bar)) ||
		    (npk_reg_read_no_lock(msu_reg->mscsts, &sts)))
			return -EFAULT;

		if (cmwp == 0)
			return -EBUSY;

		num_bytes = cmwp - (bar << 12);
		msu->wrapstat = sts >> 1 & 0x1;
		if (!msu->wrapstat) {
			msu->len = num_bytes;
		} else {
			msu->len = msu->size;
			msu->left = msu->size - num_bytes;
			msu->right = num_bytes;
		}
	}

	if (msu->len <= msu->readpos)
		return -EBUSY;

	len = msu->len - msu->readpos;
	if (cnt < len)
		len = cnt;

	msu_mem = (u8 *)msu->mem;

	if (!msu->wrapstat) {
		start = msu_mem + msu->readpos;
		if (copy_to_user((void __user *)ubuff, start, len))
			return -EFAULT;
	} else {
		if (msu->readpos >= msu->left) {
			/* one copy */
			start = msu_mem + msu->readpos - msu->left;
			if (copy_to_user((void __user *)ubuff, start, len))
				return -EFAULT;
		} else {
			if (msu->readpos + len <= msu->left) {
				/* one copy */
				start = msu_mem + msu->right + msu->readpos;
				if (copy_to_user((void __user *)ubuff, start, len))
					return -EFAULT;
			 } else {
				/* two copies */
				u32 len1 = msu->left - msu->readpos;
				u32 len2 = len - len1;

				start = msu_mem + msu->right + msu->readpos;
				if (copy_to_user((void __user *)ubuff, start, len1))
					return -EFAULT;

				if (copy_to_user((void __user *)ubuff + len1, msu_mem, len2))
					return -EFAULT;
			}
		}
	}

	msu->readpos += len;
	return len;
}

/**
 * This function allows to read the CSR-driven mode buffer that has
 * been previously allocated with NPKIOC_ALLOC_BUFF ioctl command.
 */
static ssize_t
npk_trace_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	ssize_t ret;
	struct npk_pci_device *npk_dev = get_npk_dev();

	if (!npk_dev)
		return -ENODEV;

	if (!msu_csr || msu_csr->mode != MSU_MODE_CSR) {
		pr_err("no msu allocated yet\n");
		put_npk_dev();
		return -EFAULT;
	}

	if (!cnt) {
		put_npk_dev();
		return 0;
	}

	ret = trace_data_to_user(npk_dev, msu_csr, ubuf, cnt);
	if (ret != -EBUSY) {
		put_npk_dev();
		return ret;
	}

	msu_csr->len = 0;
	msu_csr->readpos = 0;
	msu_csr->wrapstat = 0;

	put_npk_dev();
	return 0;
}

/* This function assumes the caller has grabbed the mutex
 * that protects the npk device
 */
static int ubuff_map(struct vm_area_struct *vma, unsigned long addr,
		     struct msu_ctx *msu)
{
	struct msu_window *win;
	int i, j;
	int err;

	for (i = 0; i < msu->nr_of_wins; i++) {
		win = &msu->win_list[i];

		for (j = 0; j < win->nr_of_blks; j++) {
			struct page *page = virt_to_page(win->blk_list[j].blk);
			err = vm_insert_page(vma, addr, page);
			if (err)
				return err;
			addr += PAGE_SIZE;
		}
	}

	return 0;
}

/**
 * Memory mapping of the linked-list mode buffer that has been
 * previously allocated with the NPKIOC_ALLOC_WINDOW ioctl command.
 */
static int npk_trace_mmap(struct file *filep, struct vm_area_struct *vma)
{
	struct npk_pci_device *npk_dev;
	unsigned long addr;
	int ret;

	if (!vma)
		return -ENXIO;

	if (vma->vm_end < vma->vm_start)
		return -EINVAL;

	npk_dev = get_npk_dev();
	if (!npk_dev)
		return -ENODEV;

	if (!msu_win || !msu_win->size) {
		put_npk_dev();
		return -EINVAL;
	}

	if (vma->vm_end - vma->vm_start < msu_win->size) {
		pr_err("mmap size must be %u, not %lu\n", msu_win->size, vma->vm_end - vma->vm_start);
		put_npk_dev();
		return -EINVAL;
	}

	addr = vma->vm_start;
	ret = ubuff_map(vma, addr, msu_win);

	put_npk_dev();
	return ret;
}

static long npk_trace_ioctl(struct file *filp, unsigned cmd, unsigned long arg)
{
	struct npk_pci_device *npk_dev = get_npk_dev();
	int ret;

	if (!npk_dev)
		return -ENODEV;

	switch (cmd) {
	case NPKIOC_ALLOC_BUFF:
		ret = alloc_buffer(npk_dev, arg);
		break;
	case NPKIOC_FREE_BUFF:
		ret = free_buffer(npk_dev, arg);
		break;
	case NPKIOC_ALLOC_WINDOW:
		ret = alloc_window(npk_dev, arg);
		break;
	case NPKIOC_FREE_WINDOW:
		ret = free_window(npk_dev, arg);
		break;
	case NPKIOC_PRINT_MSU_INFO:
		ret = print_msu_info(arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	put_npk_dev();
	return ret;
}

#ifdef CONFIG_COMPAT
static long npk_trace_compat_ioctl(struct file *file, uint command, ulong u)
{
	return npk_trace_ioctl(file, command, (ulong)compat_ptr(u));
}
#else
#define npk_trace_compat_ioctl NULL
#endif

static const struct file_operations npk_trace_fops = {
	.read = npk_trace_read,
	.unlocked_ioctl = npk_trace_ioctl,
	.compat_ioctl = npk_trace_compat_ioctl,
	.mmap = npk_trace_mmap
};

static struct miscdevice npk_trace = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "npk_trace",
	.fops = &npk_trace_fops,
};

static int __init npk_trace_init(void)
{
	return misc_register(&npk_trace);
}

static void __exit npk_trace_exit(void)
{
	int idx;
	struct npk_pci_device *npk_dev = get_npk_dev();

	if (npk_dev) {
		/* Make sure all memory that may have been allocated is freed */
		for (idx = 0; idx < MAX_NUM_MSU; idx++) {
			free_winmem(npk_dev, idx);
			npk_free_kmem(npk_dev, idx);
		}
		put_npk_dev();
	}

	misc_deregister(&npk_trace);
}

module_init(npk_trace_init);
module_exit(npk_trace_exit);

MODULE_AUTHOR("Kai Li <kai.li@intel.com>");
MODULE_DESCRIPTION("Intel NPK trace driver");
MODULE_LICENSE("GPL v2");


