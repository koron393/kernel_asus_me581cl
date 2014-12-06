/*
 * platform_slimport.c: slimport platform data initilization file
 */

#include "platform_slimport.h"
#include <asm/intel-mid.h>
#include <linux/i2c.h>
#include <linux/platform_data/slimport_device.h>

#define SP_TX_PWR_V10_CTRL		(45)		//AP IO Control - Power+V10
#define SP_TX_HW_RESET			(46)		//AP IO Control - Reset
#define SLIMPORT_CABLE_DETECT	(177)	//AP IO Input - Cable detect
#define SP_TX_CHIP_PD_CTRL		(55)		//AP IO Control - CHIP_PW_HV

static struct anx7814_platform_data anx7814_pdata =
{
	.gpio_p_dwn = SP_TX_CHIP_PD_CTRL,
	.gpio_reset = SP_TX_HW_RESET,
	.gpio_cbl_det = SLIMPORT_CABLE_DETECT,
	.gpio_v10_ctrl = SP_TX_PWR_V10_CTRL,
	.gpio_v33_ctrl = NULL,
	.external_ldo_control = 1,
};

void *slimport_platform_data(void *info)
{
	return &anx7814_pdata;
}