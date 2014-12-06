/* Copyright (c) 2014, ASUSTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/board_asustek.h>

#ifdef CONFIG_ASUSTEK_PCBID
static char serialno[32] = {0,};
int __init asustek_androidboot_serialno(char *s)
{
	int n;

	if (*s == '=')
		s++;
	n = snprintf(serialno, sizeof(serialno), "%s", s);
	serialno[n] = '\0';

	return 1;
}
__setup("androidboot.serialno", asustek_androidboot_serialno);

static struct resource resources_asustek_pcbid[] = {
	{
		.start	= 163,
		.end	= 163,
		.name	= "PCB_ID0",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 97,
		.end	= 97,
		.name	= "PCB_ID1",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 154,
		.end	= 154,
		.name	= "PCB_ID2",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 155,
		.end	= 155,
		.name	= "PCB_ID3",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 156,
		.end	= 156,
		.name	= "PCB_ID4",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 157,
		.end	= 157,
		.name	= "PCB_ID5",
		.flags	= IORESOURCE_IO,
	},
};

static struct resource pdata_resources0[] = {
	{
		.start	= 158,
		.end	= 158,
		.name	= "PCB_ID6",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 160,
		.end	= 160,
		.name	= "PCB_ID7",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 161,
		.end	= 161,
		.name	= "PCB_ID8",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 0,
		.end	= 0,
		.name	= "PCB_ID9",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 1,
		.end	= 1,
		.name	= "PCB_ID10",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 2,
		.end	= 2,
		.name	= "PCB_ID11",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 3,
		.end	= 3,
		.name	= "PCB_ID12",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 4,
		.end	= 4,
		.name	= "PCB_ID13",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 5,
		.end	= 5,
		.name	= "PCB_ID14",
		.flags	= IORESOURCE_IO,
	},
};

static struct resource pdata_resources1[] = {
	{
		.start	= 158,
		.end	= 158,
		.name	= "PCB_ID6",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 160,
		.end	= 160,
		.name	= "PCB_ID7",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 161,
		.end	= 161,
		.name	= "PCB_ID8",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 159,
		.end	= 159,
		.name	= "PCB_ID9",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 1,
		.end	= 1,
		.name	= "PCB_ID10",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 2,
		.end	= 2,
		.name	= "PCB_ID11",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 0,
		.end	= 0,
		.name	= "PCB_ID12",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 3,
		.end	= 3,
		.name	= "PCB_ID13",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 4,
		.end	= 4,
		.name	= "PCB_ID14",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 5,
		.end	= 5,
		.name	= "PCB_ID15",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 15,
		.end	= 15,
		.name	= "PCB_ID16",
		.flags	= IORESOURCE_IO,
	},
};

static struct resource pdata_resources2[] = {
	{
		.start	= 158,
		.end	= 158,
		.name	= "PCB_ID6",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 160,
		.end	= 160,
		.name	= "PCB_ID7",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 161,
		.end	= 161,
		.name	= "PCB_ID8",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 0,
		.end	= 0,
		.name	= "PCB_ID9",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 3,
		.end	= 3,
		.name	= "PCB_ID10",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 4,
		.end	= 4,
		.name	= "PCB_ID11",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 5,
		.end	= 5,
		.name	= "PCB_ID12",
		.flags	= IORESOURCE_IO,
	},
};


struct asustek_pcbid_platform_data asustek_pcbid_pdata = {
	.UUID = serialno,
	.resource0 = pdata_resources0,
	.nr_resource0 = ARRAY_SIZE(pdata_resources0),
	.resource1 = pdata_resources1,
	.nr_resource1 = ARRAY_SIZE(pdata_resources1),
	.resource2 = pdata_resources2,
	.nr_resource2 = ARRAY_SIZE(pdata_resources2),
};

static struct platform_device asustek_pcbid_device = {
	.name		= "asustek_pcbid",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_asustek_pcbid),
	.resource = resources_asustek_pcbid,
	.dev = {
		.platform_data = &asustek_pcbid_pdata,
	}
};

static void __init asustek_add_pcbid_devices(void)
{
	platform_device_register(&asustek_pcbid_device);
}

rootfs_initcall(asustek_add_pcbid_devices);
#endif
