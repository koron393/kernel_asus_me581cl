/*
  * drivers/power/Smb345-charger.c
  *
  * Charger driver for Summit SMB345
  *
  * Copyright (c) 2012, ASUSTek Inc.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful, but WITHOUT
  * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  * more details.
  */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/smb345-charger.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/usb/otg.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/board_asustek.h>
#include <linux/earlysuspend.h>

#include <asm/uaccess.h>

#define smb345_CHARGE		0x00
#define smb345_CHRG_CRNTS	0x01
#define smb345_VRS_FUNC		0x02
#define smb345_FLOAT_VLTG	0x03
#define smb345_CHRG_CTRL	0x04
#define smb345_STAT_TIME_CTRL	0x05
#define smb345_PIN_CTRL		0x06
#define smb345_THERM_CTRL	0x07
#define smb345_SYSOK_USB3	0x08
#define smb345_CTRL_REG		0x09

#define smb345_OTG_TLIM_REG	0x0A
#define smb345_HRD_SFT_TEMP	0x0B
#define smb345_FAULT_INTR	0x0C
#define smb345_STS_INTR_1	0x0D
#define smb345_I2C_ADDR  	0x0E
#define smb345_IN_CLTG_DET	0x10
#define smb345_STS_INTR_2	0x11

/* Command registers */
#define smb345_CMD_REG		0x30
#define smb345_CMD_REG_B	0x31
#define smb345_CMD_REG_c	0x33

/* Interrupt Status registers */
#define smb345_INTR_STS_A	0x35
#define smb345_INTR_STS_B	0x36
#define smb345_INTR_STS_C	0x37
#define smb345_INTR_STS_D	0x38
#define smb345_INTR_STS_E	0x39
#define smb345_INTR_STS_F	0x3A

/* Status registers */
#define smb345_STS_REG_A	0x3B
#define smb345_STS_REG_B	0x3C
#define smb345_STS_REG_C	0x3D
#define smb345_STS_REG_D	0x3E
#define smb345_STS_REG_E	0x3F

/* GPIO pin definition */
#define APQ_AP_CHAR             22
#define APQ_AP_ACOK             179
#define CHG_I2C_SW_IN           50

#define smb345_ENABLE_WRITE	1
#define smb345_DISABLE_WRITE	0
#define ENABLE_WRT_ACCESS	0x80
#define ENABLE_APSD		0x04
#define PIN_CTRL		0x10
#define PIN_ACT_LOW      	0x20
#define ENABLE_CHARGE		0x02
#define USBIN	         	0x80
#define APSD_OK	        	0x08
#define APSD_RESULT		0x07
#define FLOAT_VOLT_MASK  	0x3F
#define ENABLE_PIN_CTRL_MASK    0x60
#define HOT_LIMIT_MASK          0x30
#define BAT_OVER_VOLT_MASK      0x40
#define STAT_OUTPUT_EN		0x20
#define GPIO_AC_OK		APQ_AP_ACOK
#define BAT_Cold_Limit 0
#define BAT_Hot_Limit 55
#define BAT_Mid_Temp_Wired 50
#define FLOAT_VOLT              0x2A
#define FLOAT_VOLT_LOW          0x1E
#define FLOAT_VOLT_43V          0x28
#define FLOAT_VOLT_LOW_DECIMAL 4110000
#define THERMAL_RULE1 1
#define THERMAL_RULE2 2

/* 581 setting definition */
#define BATTERY_OV             	 	0x02
#define FAST_CHARGE_CURRENT    	 	0xE0
#define TERMINATION_CURRENT   		0x07
#define FLOAT_VOLT_411V                 0x1E
#define FLOAT_VOLT_42V      		0x23
#define FLOAT_VOLT_435V      		0x2B
#define VBATT_100MV           		0x80
#define VFLT_240MV             		0x18
#define SOFT_HOT_LIMIT_BEHAVIOR_NO_RESPONSE  0x03
#define SOFT_HOT_LIMIT_BEHAVIOR_COMPENSATION 0x02
#define MAX_DCIN 1
#define MAX_USBIN 0
#define REVERTBYTE(a)   (char)((((a)>>4) & 0x0F) |(((a)<<4) & 0xF0))
#define USB_TO_HC_MODE          	    0x03
#define USB_to_REGISTER_CONTROL 	    0x10
#define AC_CURRENT			    1200
#define USB_CURRENT		            500
#define HARD_HOT_LIMIT_SUSPEND  	    0x30
#define SOFT_HOT_AND_COLD_LIMIT_SUSPEND     0x06
#define SOFT_HOT_AND_COLD_LIMIT_NO_RESPONSE 0x0F
#define HARD_HOT_59_SOFT_HOT_53_MASK        0xCC
#define HARD_HOT_59_SOFT_HOT_53             0x12

/* Project definition */
#define ME581CL 0
#define FE375CG 1
#define ME572C  2
#define ME572CL 3

/* Functions declaration */
extern int  bq27541_battery_callback(unsigned usb_cable_state);
//extern void touch_callback(unsigned cable_status);
static ssize_t smb345_reg_show(struct device *dev, struct device_attribute *attr, char *buf);

/* Global variables */
static struct smb345_charger *charger;
static struct workqueue_struct *smb345_wq;
struct wake_lock charger_wakelock;
struct wake_lock COS_wakelock;
static int charge_en_flag = 1;
bool otg_on = false;
bool JEITA_early_suspend = false;
extern int ac_on;
extern int usb_on;
static bool disable_DCIN;
static unsigned int cable_state_detect = CHARGER_NONE;
extern int cable_status_register_client(struct notifier_block *nb);
extern int cable_status_unregister_client(struct notifier_block *nb);
extern unsigned int query_cable_status(void);
unsigned int smb345_charger_status = 0;
static bool smb345init = false;
static bool JEITA_init = false;
extern bool smb347_shutdown = false;
static DEFINE_MUTEX(smb347_shutdown_mutex);

/* ATD commands */
static ssize_t smb345_input_AICL_result_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t show_smb345_charger_status(struct device *dev, struct device_attribute *attr, char *buf);

/* Sysfs interface */
static DEVICE_ATTR(reg_status, S_IWUSR | S_IRUGO, smb345_reg_show, NULL);
static DEVICE_ATTR(input_AICL_result, S_IWUSR | S_IRUGO, smb345_input_AICL_result_show,NULL);
static DEVICE_ATTR(smb345_status, S_IWUSR | S_IRUGO, show_smb345_charger_status,NULL);

static struct attribute *smb345_attributes[] = {
	&dev_attr_reg_status.attr,
	&dev_attr_input_AICL_result.attr,
	&dev_attr_smb345_status.attr,
NULL
};

static const struct attribute_group smb345_group = {
	.attrs = smb345_attributes,
};

extern char mode[32] = {0,};
int __init asustek_androidboot_mode(char *s)
{
       int n;

       if (*s == '=')
               s++;
       n = snprintf(mode, sizeof(mode), "%s", s);
       mode[n] = '\0';

       return 1;
}
__setup("androidboot.mode", asustek_androidboot_mode);

static int smb345_read(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int smb345_write(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int smb345_update_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret, retval;

	retval = smb345_read(client, reg);
	if (retval < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, retval);
		return retval;
	}

	ret = smb345_write(client, reg, retval | value);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}

	return ret;
}

static int smb345_clear_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret, retval;

	retval = smb345_read(client, reg);
	if (retval < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, retval);
		return retval;
	}

	ret = smb345_write(client, reg, retval & (~value));
	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}

	return ret;
}

int smb345_volatile_writes(struct i2c_client *client, uint8_t value)
{
	int ret = 0;

	if (value == smb345_ENABLE_WRITE) {
		/* Enable volatile write to config registers */
		ret = smb345_update_reg(client, smb345_CMD_REG,
						ENABLE_WRT_ACCESS);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): Failed in writing"
				"register 0x%02x\n", __func__, smb345_CMD_REG);
			return ret;
		}
	} else {
		ret = smb345_read(client, smb345_CMD_REG);
		if (ret < 0) {
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
			return ret;
		}

		ret = smb345_write(client, smb345_CMD_REG, ret & (~(1<<7)));
		if (ret < 0) {
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
			return ret;
		}
	}
	return ret;
}

int smb345_setting_series(struct i2c_client *client)
{
        u8 ret = 0, setting;

        ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() error in smb345 volatile writes \n", __func__);
                goto error;
        }

        /* set battery OV does not end charge cycle */
        ret = smb345_clear_reg(client, smb345_VRS_FUNC, BATTERY_OV);
        if (ret < 0) {
                dev_err(&client->dev, "%s() error in set battery OV \n", __func__);
                goto error;
        }

        /* set Fast Charger current 2A */
        ret = smb345_update_reg(client, smb345_CHARGE, FAST_CHARGE_CURRENT);
        if (ret < 0) {
                dev_err(&client->dev, "%s() error in set Fast Charger \n", __func__);
                goto error;
        }

        /* set cold soft limit current 600mA */
        ret = smb345_update_reg(client, smb345_OTG_TLIM_REG, 0x80);
        if (ret < 0) {
                dev_err(&client->dev, "%s() error in set cold soft limit \n", __func__);
                goto error;
        }

        ret = smb345_clear_reg(client, smb345_OTG_TLIM_REG, 0x40);
        if (ret < 0) {
                dev_err(&client->dev, "%s() error in set cold soft limit \n", __func__);
                goto error;
        }

        /* set termination current 200mA*/
        ret = smb345_update_reg(client, smb345_CHARGE, TERMINATION_CURRENT);
        if (ret < 0) {
                dev_err(&client->dev, "%s() error in set termination current \n", __func__);
                goto error;
        }

        setting = smb345_read(client, smb345_FLOAT_VLTG);
        if (ret < 0) {
                dev_err(&client->dev, "%s() error in smb345 read \n", __func__);
                goto error;
        }

        ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() error in smb345 volatile writes \n", __func__);
                goto error;
        }

error:
        return ret;
}

/* set USB to HC mode and USB5/1/HC to register control */

int smb345_setting_USB_to_HCmode(struct i2c_client *client)
{
	u8 ret = 0;

	ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() error in smb345 volatile writes \n", __func__);
		goto error;
	}

	ret = smb345_update_reg(client, smb345_CMD_REG_B, USB_TO_HC_MODE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() error in set USB to HC mode \n", __func__);
		goto error;
	}

        ret = smb345_clear_reg(client, smb345_PIN_CTRL, USB_to_REGISTER_CONTROL);
        if (ret < 0) {
                dev_err(&client->dev, "%s() error in smb345 read \n", __func__);
                goto error;
        }

	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() error in smb345 volatile writes \n", __func__);
		goto error;
	}

error:
	return ret;
}

static int smb345_pin_control(bool state)
{
	struct i2c_client *client = NULL;
	u8 ret = 0;

	mutex_lock(&charger->pinctrl_lock);

        if (charger == NULL || smb345init == false) {
                return 0;
        }
	client = charger->client;

	if (state) {
		/*Pin Controls - active low */
		ret = smb345_update_reg(client, smb345_PIN_CTRL, PIN_ACT_LOW);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): Failed to"
						"enable charger\n", __func__);
		}
	} else {
		/*Pin Controls - active high */
		ret = smb345_clear_reg(client, smb345_PIN_CTRL, PIN_ACT_LOW);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): Failed to"
						"disable charger\n", __func__);
		}
	}

	mutex_unlock(&charger->pinctrl_lock);
	return ret;
}

int smb345_charger_enable(bool state)
{
	u8 ret = 0;
	struct i2c_client *client = NULL;

	if (charger == NULL || smb345init == false) {
		return 0;
	}
	client = charger->client;

	ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() error in configuring charger..\n",
								__func__);
		goto error;
	}
	charge_en_flag = state;
	smb345_pin_control(state);

	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() error in configuring charger..\n",
								__func__);
		goto error;
	}

error:
	return ret;
}
EXPORT_SYMBOL_GPL(smb345_charger_enable);

int
smb345_set_InputCurrentlimit(struct i2c_client *client, u32 current_setting, int usb_dc)
{
       int ret = 0, retval;
       u8 setting, tempval = 0;
	project_id project = PROJECT_ID_INVALID;
	hw_rev hw_version = HW_REV_INVALID;

	hw_version = asustek_get_hw_rev();
	project = asustek_get_project_id();


        wake_lock(&charger_wakelock);

        ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() error in configuring charger..\n",
                                                                __func__);
                goto error;
        }

        if (charge_en_flag)
                smb345_pin_control(0);

        retval = smb345_read(client, smb345_VRS_FUNC);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_VRS_FUNC);
                goto error;
        }

        setting = retval & (~(BIT(4)));
        SMB_NOTICE("Disable AICL, retval=%x setting=%x\n", retval, setting);
        ret = smb345_write(client, smb345_VRS_FUNC, setting);
        if (ret < 0) {
                dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
                        "0x%02x\n", __func__, setting, smb345_VRS_FUNC);
                goto error;
        }
        if(current_setting != 0)
        {
                retval = smb345_read(client, smb345_CHRG_CRNTS);
                if (retval < 0) {
                        dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHRG_CRNTS);
                        goto error;
                }

                if(usb_dc == MAX_DCIN)
                        tempval = REVERTBYTE(retval);
                else if(usb_dc == MAX_USBIN)
                        tempval = retval;

		if (hw_version == 0 && project == 0) {
	                setting = tempval & 0xF0;
	                if(current_setting == 2000)
	                        setting |= 0x07;
	                else if(current_setting == 1800)
	                        setting |= 0x06;
	                else if(current_setting == 1200)
	                        setting |= 0x04;
	                else if(current_setting == 900)
	                        setting |= 0x03;
	                else if(current_setting == 700)
	                        setting |= 0x02;
	                else if(current_setting == 500)
	                        setting |= 0x01;
		} else {
	                setting = tempval & 0x0F;
	                if(current_setting == 2000)
	                        setting |= 0x70;
	                else if(current_setting == 1800)
	                        setting |= 0x60;
	                else if(current_setting == 1200)
	                        setting |= 0x40;
	                else if(current_setting == 900)
	                        setting |= 0x30;
	                else if(current_setting == 700)
	                        setting |= 0x20;
	                else if(current_setting == 500)
	                        setting |= 0x10;
		}

                if(usb_dc == MAX_DCIN)
                        setting = REVERTBYTE(setting);

                SMB_NOTICE("Set ICL=%u retval =%x setting=%x\n", current_setting, retval, setting);

                ret = smb345_write(client, smb345_CHRG_CRNTS, setting);
                if (ret < 0) {
                        dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
                                "0x%02x\n", __func__, setting, smb345_CHRG_CRNTS);
                        goto error;
                }

                if(current_setting == 2000)
                        charger->curr_limit = 2000;
                else if(current_setting == 1800)
                        charger->curr_limit = 1800;
                else if(current_setting == 1200)
                        charger->curr_limit = 1200;
                else if(current_setting == 900)
                        charger->curr_limit = 900;
                else if(current_setting == 700)
                        charger->curr_limit = 700;
                else if(current_setting == 500)
                        charger->curr_limit = 500;

                if (current_setting > 900) {
                        charger->time_of_1800mA_limit = jiffies;
                } else{
                        charger->time_of_1800mA_limit = 0;
                }

                retval = smb345_read(client, smb345_VRS_FUNC);
                if (retval < 0) {
                        dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                        __func__, smb345_VRS_FUNC);
                        goto error;
                }
        }

        setting = retval | BIT(4);
        SMB_NOTICE("Re-enable AICL, setting=%x\n", setting);
        msleep(20);
        ret = smb345_write(client, smb345_VRS_FUNC, setting);
        if (ret < 0) {
                dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
                        "0x%02x\n", __func__, setting, smb345_VRS_FUNC);
                        goto error;
        }

        if (charge_en_flag)
                smb345_pin_control(1);

        ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() error in configuring charger..\n",
                                                                __func__);
                goto error;
        }

error:
        wake_unlock(&charger_wakelock);
        return ret;
}

static irqreturn_t smb345_inok_isr(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

void smb345_recheck_charging_type(void)
{
	int retval;
	struct i2c_client *client = NULL;

        if (charger == NULL) {
                return 0;
        }
	client = charger->client;

        if (ac_on) {
                retval = smb345_read(client, smb345_STS_REG_E);
                if (retval < 0) {
                        dev_err(&client->dev, "%s() AICL result reading fail \n", __func__);
                }
		SMB_NOTICE("smb345 AICL result=%x\n", retval);
                if (retval != 0x14) {
                        SMB_NOTICE("reconfig input current\n");
                        smb345_set_InputCurrentlimit(client, 1200, MAX_USBIN);
//                      smb345_setting_USB_to_HCmode(client);
                }
        }

}
EXPORT_SYMBOL(smb345_recheck_charging_type);

static int smb345_inok_irq(struct smb345_charger *smb)
{

	/* cable detect from usb drivers */
	/*
	int err = 0 ;
	unsigned gpio = GPIO_AC_OK;
	unsigned irq_num = gpio_to_irq(gpio);

	err = gpio_request(gpio, "smb345_inok");
	if (err) {
		SMB_ERR("gpio %d request failed \n", gpio);
	}

	err = gpio_direction_input(gpio);
	if (err) {
		SMB_ERR("gpio %d unavaliable for input \n", gpio);
	}

	err = request_irq(irq_num, smb345_inok_isr, IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING|IRQF_SHARED,
		"smb345_inok", smb);
	if (err < 0) {
		SMB_ERR("%s irq %d request failed \n","smb345_inok", irq_num);
		goto fault ;
	}
	enable_irq_wake(irq_num);
	SMB_NOTICE("GPIO pin irq %d requested ok, smb345_INOK = %s\n", irq_num, gpio_get_value(gpio)? "H":"L");
	return 0;

fault:
	gpio_free(gpio);
	return err;
	*/
	return 0;
}

static int smb345_configure_otg(struct i2c_client *client)
{
	int ret = 0;
	project_id project = PROJECT_ID_INVALID;
	hw_rev hw_version = HW_REV_INVALID;

	hw_version = asustek_get_hw_rev();
	project = asustek_get_project_id();

	/* Allow Violate Register can be written and disable OTG */
	ret = smb345_update_reg(client, smb345_CMD_REG, 0x80);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing"
			"register 0x%02x\n", __func__, smb345_CMD_REG);
		goto error;
	}

	ret = smb345_clear_reg(client, smb345_CMD_REG, 0x10);
        if (ret < 0) {
                dev_err(&client->dev, "%s(): Failed in writing"
                        "register 0x%02x\n", __func__, smb345_CMD_REG);
                goto error;
        }

	/* Switching Frequency change to 1.5Mhz */
        ret = smb345_update_reg(client, smb345_THERM_CTRL, 0x80);
        if (ret < 0) {
                dev_err(&client->dev, "%s(): Failed in writing"
                        "register 0x%02x\n", __func__, smb345_THERM_CTRL);
                goto error;
        }

	/* Change "OTG output current limit" to 250mA */
		/* For smb347 */

	if (hw_version == 0 && project == 0) {
		ret = smb345_write(client, smb345_OTG_TLIM_REG, 0x34);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): Failed in writing"
				"register 0x%02x\n", __func__, smb345_OTG_TLIM_REG);
			goto error;
		}

		/* For smb358 */
	} else {
		ret = smb345_clear_reg(client, smb345_OTG_TLIM_REG, 0x0c);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): Failed in writing"
				"register 0x%02x\n", __func__, smb345_OTG_TLIM_REG);
			goto error;
		}
	}

	/* Enable OTG */
        ret = smb345_update_reg(client, smb345_CMD_REG, 0x10);
        if (ret < 0) {
		dev_err(&client->dev, "%s: Failed in writing register"
			"0x%02x\n", __func__, smb345_CMD_REG);
		goto error;
	}

	/* Change "OTG output current limit" from 250mA to 500mA */
		/* For smb347 */
	if (hw_version == 0 && project == 0) {
	        ret = smb345_write(client, smb345_OTG_TLIM_REG, 0x0C);
                if (ret < 0) {
                        dev_err(&client->dev, "%s(): Failed in writing"
                                "register 0x%02x\n", __func__, smb345_OTG_TLIM_REG);
                        goto error;
                }

		/* For smb358 */
        } else {
                ret = smb345_update_reg(client, smb345_OTG_TLIM_REG, 0x04);
                if (ret < 0) {
                        dev_err(&client->dev, "%s(): Failed in writing"
                                "register 0x%02x\n", __func__, smb345_OTG_TLIM_REG);
                        goto error;
                }
        }

	/* Disable volatile writes to registers */
	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s error in configuring OTG..\n",
								__func__);
		goto error;
	}

	return 0;

error:
	return ret;
}

void smb345_otg_status(bool on)
{
	int ret;
	struct i2c_client *client = NULL;

	otg_on = on;
	if (charger == NULL) {
		return 0;
	}
	client = charger->client;

	SMB_NOTICE("otg function: %s\n", on ? "on" : "off");

	if (on) {
		otg_on = true;
		/* ENABLE OTG */
		ret = smb345_configure_otg(client);
		if (ret < 0) {
			dev_err(&client->dev, "%s() error in configuring"
				"otg..\n", __func__);
			return;
		}
	} else
		otg_on = false;

}
EXPORT_SYMBOL_GPL(smb345_otg_status);

int smb345_otg_complete(struct i2c_client *client)
{
	int ret;

        ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

	/* Change "OTG output current limit" to 250mA */
	ret = smb345_clear_reg(client, smb345_OTG_TLIM_REG, 0x0c);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing"
			"register 0x%02x\n", __func__, smb345_OTG_TLIM_REG);
		goto error;
	}

	smb345_otg_status(false);

	/* IC Reset to default */
	ret = smb345_update_reg(client, smb345_CMD_REG_B, 0x80);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing"
			"register 0x%02x\n", __func__, smb345_OTG_TLIM_REG);
		goto error;
	}

	/* Disable volatile writes to registers */
	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s error in configuring OTG..\n",
								__func__);
		goto error;
	}
	return 0;
error:
	return ret;
}

static void workqueue_setting_input_current(struct work_struct *work)
{
        struct smb345_charger *charger = container_of(work, struct smb345_charger, cable_det_work.work);
	struct i2c_client *client = charger->client;
	int retval = 0;

	if (cable_state_detect == CHARGER_NONE){
		if (otg_on == true){
			smb345_otg_complete(client);
		}
	} else if (cable_state_detect == CHARGER_CDP || cable_state_detect == CHARGER_DCP || cable_state_detect == CHARGER_ACA) {
#ifndef FACTORY_IMAGE
		smb345_setting_series(client);
#endif
		charger->cur_cable_type = ac_cable;

		/* porting guide IUSB_IN (01h[7:4]) < 1200 mA */
		retval = smb345_read(client, smb345_CHRG_CRNTS);
                if (retval < 0){
                        dev_err(&client->dev, "%s() IUSB_IN reads fail \n", __func__);
                }
		retval = (retval & 0xF0);
		if (retval < 0x40){
			SMB_NOTICE("IUSB_IN=%x\n", retval);
			smb345_set_InputCurrentlimit(client, AC_CURRENT, MAX_USBIN);
		}
//		smb345_setting_USB_to_HCmode(client);
	} else if (cable_state_detect == CHARGER_SDP) {

#ifndef FACTORY_IMAGE
		smb345_setting_series(client);
#endif
		charger->cur_cable_type = usb_cable;
		smb345_set_InputCurrentlimit(client, USB_CURRENT, MAX_USBIN);
	} else if (cable_state_detect == CHARGER_OTG) {
		smb345_otg_status(true);
	}
}

int smb345_float_volt_set(unsigned int val)
{
	struct i2c_client *client = charger->client;
	int ret = 0, retval;

	if (val > 4500 || val < 3500) {
		SMB_ERR("%s(): val=%d is out of range !\n",__func__, val);
	}

	SMB_NOTICE("%s(): val=%d\n", __func__, val);

	ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
		goto fault;
	}

	retval = smb345_read(client, smb345_FLOAT_VLTG);
	if (retval < 0) {
		dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
				__func__, smb345_FLOAT_VLTG);
		goto fault;
	}
	retval = retval & (~FLOAT_VOLT_MASK);
	val = clamp_val(val, 3500, 4500) - 3500;
	val /= 20;
	retval |= val;
	ret = smb345_write(client, smb345_FLOAT_VLTG, retval);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing"
			"register 0x%02x\n", __func__, smb345_FLOAT_VLTG);
		goto fault;
       }

	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() charger disable write error..\n", __func__);
	}
	return 0;
fault:
	return ret;
}
EXPORT_SYMBOL_GPL(smb345_float_volt_set);


/*
int apsd_cable_type_detect(void)
{
        struct i2c_client *client = charger->client;
        u8 retval;
        int  success = 0;
        int ac_ok = GPIO_AC_OK;

        mutex_lock(&charger->apsd_lock);

        if (gpio_get_value(ac_ok)) {
                SMB_NOTICE("INOK=H\n");
                charger->cur_cable_type = non_cable;
                success =  bq27541_battery_callback(non_cable);
//                touch_callback(non_cable);
        } else {
                SMB_NOTICE("INOK=L\n");

		smb345_setting_series(client);
                retval = smb345_read(client, smb345_STS_REG_E);
                if (retval & USBIN) {
                        retval = smb345_read(client, smb345_STS_REG_D);
                        if (retval & APSD_OK) {
                                retval &= APSD_RESULT;
                                if (retval == apsd_SDP) {
                                        SMB_NOTICE("Cable: SDP\n");
                                        charger->cur_cable_type = usb_cable;
                                        smb345_set_InputCurrentlimit(client, 500, MAX_USBIN);
                                        success =  bq27541_battery_callback(usb_cable);
//                                        touch_callback(usb_cable);
                                } else {
                                        if (retval == apsd_CDP) {
                                                SMB_NOTICE("Cable: CDP\n");
                                        } else if (retval == apsd_DCP) {
                                                SMB_NOTICE("Cable: DCP\n");
                                        } else if (retval == apsd_OCP) {
                                                SMB_NOTICE("Cable: OTHER\n");
                                        } else if (retval == apsd_ACA) {
                                                SMB_NOTICE("Cable: ACA\n");
                                        } else {
                                                SMB_NOTICE("Cable: TBD\n");
                                                charger->cur_cable_type = unknow_cable;
                                                if(cable_state_detect == CHARGER_CDP || \
                                                        cable_state_detect == CHARGER_DCP || \
                                                        cable_state_detect == CHARGER_OTHER || \
                                                        cable_state_detect == CHARGER_ACA)
                                                {
                                                        SMB_NOTICE("Change TBD type to ac\n");
                                                        charger->cur_cable_type = ac_cable;
                                                        success = bq27541_battery_callback(ac_cable);
//                                                      touch_callback(ac_cable);
                                                }
                                                else if(cable_state_detect == CHARGER_SDP)
                                                {
                                                        SMB_NOTICE("Change TBD type to usb\n");
                                                        charger->cur_cable_type = usb_cable;
                                                        smb345_set_InputCurrentlimit(client, 500, MAX_USBIN);
                                                        success = bq27541_battery_callback(usb_cable);
//                                                      touch_callback(usb_cable);
                                                }
                                                goto done;
                                        }
                                        charger->cur_cable_type = ac_cable;
					smb345_set_InputCurrentlimit(client, 1800, MAX_USBIN);
					smb345_setting_USB_to_HCmode(client);
					success =  bq27541_battery_callback(ac_cable);
//                                      touch_callback(ac_cable);
                                }
                        } else {
                                charger->cur_cable_type = unknow_cable;
                                SMB_NOTICE("APSD Not Completed\n");
                        }
                } else {
                        charger->cur_cable_type = unknow_cable;
                        SMB_NOTICE("USB Input Not in Use\n");
                }
        }

done:
        charger->old_cable_type = charger->cur_cable_type;

        mutex_unlock(&charger->apsd_lock);
        return success;
}
*/


int cable_status_notify2(struct notifier_block *self, unsigned long action, void *dev)
{

        int ret;
        struct i2c_client *client = NULL;

        if (charger == NULL) {
                return 0;
        }
        client = charger->client;

	int  success = 0;

	switch (action) {
		case POWER_SUPPLY_CHARGER_TYPE_NONE:
			printk(KERN_INFO "%s CHRG_UNKNOWN !!!\n", __func__);
			cable_state_detect = CHARGER_NONE;
			charger->cur_cable_type = unknow_cable;
			queue_work(smb345_wq,&charger->cable_det_work);
			success =  bq27541_battery_callback(non_cable);
			break;
		case POWER_SUPPLY_CHARGER_TYPE_USB_SDP:
			printk(KERN_INFO "%s CHRG_SDP !!!\n", __func__);
			cable_state_detect = CHARGER_SDP;
			queue_work(smb345_wq,&charger->cable_det_work);
                        success = bq27541_battery_callback(usb_cable);
			break;
		case POWER_SUPPLY_CHARGER_TYPE_USB_CDP:
			printk(KERN_INFO "%s CHRG_CDP !!!\n", __func__);
			cable_state_detect = CHARGER_CDP;
			queue_work(smb345_wq,&charger->cable_det_work);
			success =  bq27541_battery_callback(ac_cable);
			break;
		case POWER_SUPPLY_CHARGER_TYPE_USB_DCP:
			printk(KERN_INFO "%s CHRG_DCP !!!\n", __func__);
			cable_state_detect = CHARGER_DCP;
			queue_work(smb345_wq,&charger->cable_det_work);
                        success =  bq27541_battery_callback(ac_cable);
			break;
		case POWER_SUPPLY_CHARGER_TYPE_ACA_DOCK:
			printk(KERN_INFO "%s CHRG_ACA !!!\n", __func__);
			cable_state_detect = CHARGER_ACA;
			queue_work(smb345_wq,&charger->cable_det_work);
			success =  bq27541_battery_callback(ac_cable);
			break;
		case POWER_SUPPLY_CHARGER_TYPE_OTG:
			printk(KERN_INFO "%s CHRG_OTG !!!\n", __func__);
			cable_state_detect = CHARGER_OTG;
			queue_work(smb345_wq,&charger->cable_det_work);
			break;
		case POWER_SUPPLY_CHARGER_TYPE_SE1:
                        printk(KERN_INFO "%s CHRG_SE1 !!!\n", __func__);
                        cable_state_detect = CHARGER_DCP;
                        queue_work(smb345_wq,&charger->cable_det_work);
                        success =  bq27541_battery_callback(ac_cable);
                        break;
		default:
			break;
	}
	return NOTIFY_OK;
}
EXPORT_SYMBOL_GPL(cable_status_notify2);

static struct notifier_block cable_status_notifier2 = {
        .notifier_call = cable_status_notify2,
};

int cable_detect_callback(unsigned int cable_state)
{
        cable_state_detect = cable_state;
        if(charger->cur_cable_type == unknow_cable)
        {
                cable_state_detect = CHARGER_NONE;
        }
        return 0;
}
EXPORT_SYMBOL_GPL(cable_detect_callback);

static void inok_isr_work_function(struct work_struct *dat)
{
	cancel_delayed_work(&charger->inok_isr_work);
}

static void cable_det_work_function(struct work_struct *dat)
{

	/* cable detect from usb driver */
	/*
        struct i2c_client *client = charger->client;

        if (delayed_work_pending(&charger->cable_det_work))
                cancel_delayed_work(&charger->cable_det_work);

        if (ac_on && !gpio_get_value(GPIO_AC_OK)) {
                int retval;
                retval = smb345_read(client, smb345_STS_REG_E);
                if (retval < 0)
                        dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                        __func__, smb345_STS_REG_E);
                else {
                        SMB_NOTICE("Status Reg E=0x%02x\n", retval);

                        if ((retval & 0xF) <= 0x1) {
                                SMB_NOTICE("reconfig input current limit\n");
                                smb345_set_InputCurrentlimit(client, 1200, MAX_USBIN);
                        }
                }
        }
	*/
}

/* Sysfs function */
static ssize_t smb345_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = charger->client;
	char tmp_buf[64];
	int i,ret;

        sprintf(tmp_buf, "SMB34x Configuration Registers Detail\n"
						"==================\n");
        strcpy(buf, tmp_buf);

	for (i = 0; i <= 14; i++) {
		ret = smb345_read(client, smb345_CHARGE+i);
		sprintf(tmp_buf, "Reg%02xh:\t0x%02x\n", smb345_CHARGE+i,ret);
		strcat(buf, tmp_buf);
	}
	for (i = 0; i <= 1; i++) {
		ret = smb345_read(client, smb345_CMD_REG+i);
		sprintf(tmp_buf, "Reg%02xh:\t0x%02x\n", smb345_CMD_REG+i,ret);
		strcat(buf, tmp_buf);
	}
	for (i = 0; i <= 10; i++) {
		ret = smb345_read(client, smb345_INTR_STS_A+i);
		sprintf(tmp_buf, "Reg%02xh:\t0x%02x\n", smb345_INTR_STS_A+i,ret);
		strcat(buf, tmp_buf);
	}

	return strlen(buf);
}

int smb345_config_thermal_suspend(void)
{
	struct i2c_client *client = charger->client;
	int ret = 0, retval, setting = 0;
	project_id project;
	project = asustek_get_project_id();

	ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
		goto error;
	}

	/* SOC Do Not Control JEITA Function & Charger Control JEITA Protection */

        retval = smb345_read(client, smb345_HRD_SFT_TEMP);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_HRD_SFT_TEMP);
                goto error;
        }

	if(project == ME572C || project == ME572CL){

	        /* Set Hard Hot Limit = 53 Deg. C */

		ret = smb345_clear_reg(client, smb345_HRD_SFT_TEMP, HARD_HOT_LIMIT_SUSPEND);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): HARD_HOT_LIMIT_SUSPEND fail\n", __func__);
			goto error;
		}
	} else {

		/* Set Hard Hot Limit = 59 Deg. C && Set Soft Hot Limit = 53 Deg. C*/

		ret = smb345_read(client, smb345_HRD_SFT_TEMP);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): HARD_HOT_LIMIT_SUSPEND fail\n", __func__);
			goto error;
		}
		ret &= HARD_HOT_59_SOFT_HOT_53_MASK;
                ret = smb345_write(client, smb345_HRD_SFT_TEMP, ret | HARD_HOT_59_SOFT_HOT_53);
                if (ret < 0) {
			dev_err(&client->dev, "%s(): HARD_HOT_LIMIT_SUSPEND fail\n", __func__);
			goto error;
		}
	}

	/* Set Soft Hot Limit Behavior = Float Voltage Compensation */
	/* Set Soft Cold temp limit = Charge Current Compensation */

	ret = smb345_update_reg(client, smb345_THERM_CTRL, SOFT_HOT_AND_COLD_LIMIT_SUSPEND);
	if (ret < 0) {
	        dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
                goto error;
        }

	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
		goto error;
	}

	smb345_charger_enable(true);
error:
	return ret;
}

int smb345_config_thermal_limit(void)
{
	struct i2c_client *client = NULL;
	int ret = 0, retval, setting = 0;

        if (charger == NULL) {
                return 0;
        }
        client = charger->client;

        ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

	retval = smb345_read(client, smb345_HRD_SFT_TEMP);
	if (retval < 0) {
		dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
				__func__, smb345_HRD_SFT_TEMP);
	}

	/* SOC Controls JEITA Function & Charger does second protection */

	setting = retval & HOT_LIMIT_MASK;
	if (setting != 0x30) {
		setting = retval & (~HOT_LIMIT_MASK);
		setting |= 0x30;
		SMB_NOTICE("Set HRD SFT limit, retval=%x setting=%x\n", retval, setting);
		ret = smb345_write(client, smb345_HRD_SFT_TEMP, setting);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
				"0x%02x\n", __func__, setting, smb345_HRD_SFT_TEMP);
		}
	} else
		SMB_NOTICE("Bypass set HRD SFT limit=%x\n", retval);

        /* set SOFT_HOT_LIMIT_BEHAVIOR */
        ret = smb345_clear_reg(client, smb345_THERM_CTRL, SOFT_HOT_AND_COLD_LIMIT_NO_RESPONSE);
        if (ret < 0) {
        dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

        ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

	return 0;
}

void JEITA_rule_1(void)
{
	/* temp < 0 ,or temp > 55 */
	/* Vchg=4.35V , Charging Disable, Recharge Voltage = Vflt-100mV, Set Fast Charge Current = 2000 mA */

        struct i2c_client *client = charger->client;
        int ret = 0, retval, setting = 0;

        ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

        /*control float voltage*/
        retval = smb345_read(client, smb345_FLOAT_VLTG);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_FLOAT_VLTG);
        }

	setting = retval & FLOAT_VOLT_MASK;
	if (setting != FLOAT_VOLT_435V) {
		setting = retval & (~FLOAT_VOLT_MASK);
		setting |= FLOAT_VOLT_435V;
		SMB_NOTICE("Set Float Volt, retval=%x setting=%x\n", retval, setting);
		ret = smb345_write(client, smb345_FLOAT_VLTG, setting);
		if (ret < 0) {
		        dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
			"0x%02x\n", __func__, setting, smb345_FLOAT_VLTG);
		}
	} else
		SMB_NOTICE("Bypass set Float Volt=%x\n", retval);

        /* Recharge Voltage = Vflt-100mV */
        retval = smb345_update_reg(client, smb345_CHRG_CRNTS, 0x04);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHRG_CRNTS);
        }

        retval = smb345_clear_reg(client, smb345_CHRG_CRNTS, 0x08);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHRG_CRNTS);
        }

	/* Set Fast Charge Current = 2000 mA */
	retval = smb345_update_reg(client, smb345_CHARGE, 0xE0);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHARGE);
        }

	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

	/* Charging Disable */
        smb345_charger_enable(false);
	SMB_NOTICE("Charger disable\n");
}

void JEITA_rule_2(void)
{
	/* 0 <= temp < 10 */
	/* Vchg=4.35V , Charging Enable, Recharge Voltage = Vflt-100mV, Set Fast Charge Current = 600 mA */

        struct i2c_client *client = charger->client;
        int ret = 0, retval, setting = 0;

        ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

        /*control float voltage*/
        retval = smb345_read(client, smb345_FLOAT_VLTG);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_FLOAT_VLTG);
        }

	setting = retval & FLOAT_VOLT_MASK;
	if (setting != FLOAT_VOLT_435V) {
		setting = retval & (~FLOAT_VOLT_MASK);
		setting |= FLOAT_VOLT_435V;
		SMB_NOTICE("Set Float Volt, retval=%x setting=%x\n", retval, setting);
		ret = smb345_write(client, smb345_FLOAT_VLTG, setting);
		if (ret < 0) {
		        dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
			"0x%02x\n", __func__, setting, smb345_FLOAT_VLTG);
		}
	} else
		SMB_NOTICE("Bypass set Float Volt=%x\n", retval);

        /* Recharge Voltage = Vflt-100mV */
        retval = smb345_update_reg(client, smb345_CHRG_CRNTS, 0x04);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHRG_CRNTS);
        }

        retval = smb345_clear_reg(client, smb345_CHRG_CRNTS, 0x08);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHRG_CRNTS);
        }

	/* Set Fast Charge Current = 600 mA */
	retval = smb345_update_reg(client, smb345_CHARGE, 0x40);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHARGE);
        }

	retval = smb345_clear_reg(client, smb345_CHARGE, 0xA0);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHARGE);
        }

        ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

        /* Charging enable */
        smb345_charger_enable(true);
	SMB_NOTICE("Charger enable\n");
}

void JEITA_rule_3(void)
{
	/* 10 <= temp < 50 */
	/* Vchg=4.35V , Charging Enable, Recharge Voltage = Vflt-100mV, Set Fast Charge Current = 2000 mA*/

        struct i2c_client *client = charger->client;
        int ret = 0, retval, setting = 0;

        ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

        /*control float voltage*/
        retval = smb345_read(client, smb345_FLOAT_VLTG);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_FLOAT_VLTG);
        }

	setting = retval & FLOAT_VOLT_MASK;
	if (setting != FLOAT_VOLT_435V) {
		setting = retval & (~FLOAT_VOLT_MASK);
		setting |= FLOAT_VOLT_435V;
		SMB_NOTICE("Set Float Volt, retval=%x setting=%x\n", retval, setting);
		ret = smb345_write(client, smb345_FLOAT_VLTG, setting);
		if (ret < 0) {
		        dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
			"0x%02x\n", __func__, setting, smb345_FLOAT_VLTG);
		}
	} else
		SMB_NOTICE("Bypass set Float Volt=%x\n", retval);

        /* Recharge Voltage = Vflt-100mV */
        retval = smb345_update_reg(client, smb345_CHRG_CRNTS, 0x04);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHRG_CRNTS);
        }

        retval = smb345_clear_reg(client, smb345_CHRG_CRNTS, 0x08);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHRG_CRNTS);
        }

	/* Set Fast Charge Current = 2000 mA */
	retval = smb345_update_reg(client, smb345_CHARGE, 0xE0);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHARGE);
        }

        ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

	/* Charging enable */
        smb345_charger_enable(true);
	SMB_NOTICE("Charger enable\n");
}

void JEITA_rule_4(void)
{
	/* 50 <= temp < 55 */
	/* Vchg=4.11V , Charging Enable, Recharge Voltage = Vflt-100mV, Set Fast Charge Current = 2000 mA*/

        struct i2c_client *client = charger->client;
        int ret = 0, retval, setting = 0;

        ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

        /*control float voltage*/
        retval = smb345_read(client, smb345_FLOAT_VLTG);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_FLOAT_VLTG);
        }

	setting = retval & FLOAT_VOLT_MASK;
	if (setting != FLOAT_VOLT_411V) {
		setting = retval & (~FLOAT_VOLT_MASK);
		setting |= FLOAT_VOLT_411V;
		SMB_NOTICE("Set Float Volt, retval=%x setting=%x\n", retval, setting);
		ret = smb345_write(client, smb345_FLOAT_VLTG, setting);
		if (ret < 0) {
		        dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
			"0x%02x\n", __func__, setting, smb345_FLOAT_VLTG);
		}
	} else
		SMB_NOTICE("Bypass set Float Volt=%x\n", retval);

	/* Recharge Voltage = Vflt-100mV */
        retval = smb345_update_reg(client, smb345_CHRG_CRNTS, 0x04);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHRG_CRNTS);
        }

	retval = smb345_clear_reg(client, smb345_CHRG_CRNTS, 0x08);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHRG_CRNTS);
        }

	/* Set Fast Charge Current = 2000 mA */
	retval = smb345_update_reg(client, smb345_CHARGE, 0xE0);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHARGE);
        }

        ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

	/* Charging enable */
        smb345_charger_enable(true);
	SMB_NOTICE("Charger enable\n");
}

void JEITA_rule_5(void)
{
	/* 50 <= temp < 55 */
	/* Vchg=4.35V , Charging Disable, Recharge Voltage = Vflt-100mV, Set Fast Charge Current = 2000 mA*/

        struct i2c_client *client = charger->client;
        int ret = 0, retval, setting = 0;

        ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

        /*control float voltage*/
        retval = smb345_read(client, smb345_FLOAT_VLTG);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_FLOAT_VLTG);
        }

	setting = retval & FLOAT_VOLT_MASK;
	if (setting != FLOAT_VOLT_435V) {
		setting = retval & (~FLOAT_VOLT_MASK);
		setting |= FLOAT_VOLT_435V;
		SMB_NOTICE("Set Float Volt, retval=%x setting=%x\n", retval, setting);
		ret = smb345_write(client, smb345_FLOAT_VLTG, setting);
		if (ret < 0) {
		        dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
			"0x%02x\n", __func__, setting, smb345_FLOAT_VLTG);
		}
	} else
		SMB_NOTICE("Bypass set Float Volt=%x\n", retval);

        /* Recharge Voltage = Vflt-100mV */
        retval = smb345_update_reg(client, smb345_CHRG_CRNTS, 0x04);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHRG_CRNTS);
        }

        retval = smb345_clear_reg(client, smb345_CHRG_CRNTS, 0x08);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHRG_CRNTS);
        }

	/* Set Fast Charge Current = 2000 mA */
	retval = smb345_update_reg(client, smb345_CHARGE, 0xE0);
        if (retval < 0) {
                dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
                                __func__, smb345_CHARGE);
        }

        ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

	/* Charging enable */
        smb345_charger_enable(false);
	SMB_NOTICE("Charger disable\n");
}

int smb345_config_thermal_charging(int temp, int volt, int rule)
{
	struct i2c_client *client = NULL;
	int ret = 0, retval, setting = 0;

	if (JEITA_early_suspend == true){
		SMB_NOTICE("JEITA_early_suspend is true, pass JEITA rule setting\n");
		return 0;
	}

	mutex_lock(&smb347_shutdown_mutex);

        if (charger == NULL) {
                return 0;
        }
        client = charger->client;

        /* JEITA flowchart start */
        smb345_config_thermal_limit();

        ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
        if (ret < 0) {
                dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
        }

	/*control float voltage*/
	retval = smb345_read(client, smb345_FLOAT_VLTG);
	if (retval < 0) {
		dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
			__func__, smb345_FLOAT_VLTG);
		goto error;
	}
	setting = retval & FLOAT_VOLT_MASK;

	SMB_NOTICE("temp=%d, volt=%d\n", temp, volt);

	if(JEITA_init == false){
		if(temp < 0){
			JEITA_rule_1();
		} else if(temp >= 0 && temp < 10){
			JEITA_rule_2();
		} else if(temp >= 10 && temp < 50){
			JEITA_rule_3();
		} else if(temp >= 50 && temp < 55){
	                if(setting == FLOAT_VOLT_435V && volt >= 4110000){
				JEITA_rule_5();
			} else {
				JEITA_rule_4();
			}
		} else if(temp >= 55){
			JEITA_rule_1();	
		} else {
			goto error;
		}

	JEITA_init = true;

	} else {
		if(temp < 0 ){
			JEITA_rule_1();
		} else if(temp > 3 && temp < 10){
			JEITA_rule_2();
		} else if(temp > 13 && temp < 47){
			JEITA_rule_3();
		} else if(temp >= 50 && temp < 52){
			if(setting == FLOAT_VOLT_435V && volt >= 4110000){
				JEITA_rule_5();
                        } else {
                                JEITA_rule_4();
                        }
		} else if(temp >= 55){
			JEITA_rule_1();
		} else {
			goto error;
		}
	}

	mutex_unlock(&smb347_shutdown_mutex);
	return 0;

error:
	mutex_unlock(&smb347_shutdown_mutex);
	SMB_NOTICE("JEITA else case : temp=%d, volt=%d\n", temp, volt);
	return ret;
}
EXPORT_SYMBOL(smb345_config_thermal_charging);

static ssize_t smb345_input_AICL_result_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = charger->client;
	int ret, index, result = 0;

	ret = i2c_smbus_read_byte_data(client, smb345_STS_REG_E);
	if (ret < 0)
		return sprintf(buf, "i2c read register fail\n");

	if(ret & 0x01)
	{
		index = ret & 0x0F;
		switch(index) {
		case 0:
			result = 300;
			break;
		case 1:
			result = 500;
			break;
		case 2:
			result = 700;
			break;
		case 3:
			result = 900;
			break;
		case 4:
			result = 1200;
			break;
		case 5:
			result = 1300;
			break;
		case 6:
			result = 1800;
			break;
		case 7:
			result = 2000;
			break;
		case 8:
			result = 2000;
			break;
		default:
			result = 2000;
		}
	}else
		return sprintf(buf, "AICL is not completed\n");

	return sprintf(buf, "%d\n", result);
}

static ssize_t show_smb345_charger_status(struct device *dev, struct device_attribute *devattr, char *buf)
{
       if(!smb345_charger_status)
               return sprintf(buf, "%d\n", 0);
       else
               return sprintf(buf, "%d\n", 1);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void smb345_early_suspend(struct early_suspend *h)
{
	SMB_NOTICE("smb345_early_suspend+\n");
	flush_workqueue(smb345_wq);
	JEITA_early_suspend = true;
	smb345_config_thermal_suspend();
	SMB_NOTICE("smb345_early_suspend-\n");
}

void smb345_late_resume(struct early_suspend *h)
{
	SMB_NOTICE("smb345_late_resume+\n");
	JEITA_early_suspend = false;
	SMB_NOTICE("smb345_late_resume-\n");
}
#endif

static int smb345_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0 ;
	unsigned gpio = CHG_I2C_SW_IN;
	project_id project = PROJECT_ID_INVALID;
	hw_rev hw_version = HW_REV_INVALID;

	hw_version = asustek_get_hw_rev();
	project = asustek_get_project_id();

	printk("smb345 probe init\n");
	/* set gpio CHG_I2C_SW_IN that PMIC to Charger */

	SMB_NOTICE("smb345 project = %d, hw_version =%d\n", project, hw_version);

	if (hw_version >= 3 && project == 0) {

		ret = gpio_request(CHG_I2C_SW_IN, "pmic_to_charger");
		if (ret) {
			SMB_ERR("gpio %d request failed\n", CHG_I2C_SW_IN);
		}

		gpio_direction_output(CHG_I2C_SW_IN, 0);
		if (ret) {
			SMB_ERR("gpio %d unavaliable for input\n", CHG_I2C_SW_IN);
		}

	SMB_NOTICE("pmic_to_charger is ok\n");

	}

	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

//	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
//		return -EIO;

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	printk("smb345_probe+\n");
	charger->client = client;
	charger->dev = &client->dev;
	i2c_set_clientdata(client, charger);

	ret = sysfs_create_group(&client->dev.kobj, &smb345_group);
	if (ret) {
		SMB_ERR("Unable to create the sysfs\n");
		goto error;
	}

	mutex_init(&charger->apsd_lock);
	mutex_init(&charger->usb_lock);
	mutex_init(&charger->pinctrl_lock);

	smb345_wq = create_singlethread_workqueue("smb345_wq");

//	INIT_DELAYED_WORK(&charger->inok_isr_work, inok_isr_work_function);
	INIT_DELAYED_WORK(&charger->cable_det_work, workqueue_setting_input_current);
	cable_status_register_client(&cable_status_notifier2);

	wake_lock_init(&charger_wakelock, WAKE_LOCK_SUSPEND,
			"charger_configuration");
	wake_lock_init(&COS_wakelock, WAKE_LOCK_SUSPEND,"COS_wakelock");
	charger->curr_limit = UINT_MAX;
	charger->cur_cable_type = non_cable;
	charger->old_cable_type = non_cable;
	disable_DCIN = false;
	smb345_charger_status = 1;
#ifdef CONFIG_HAS_EARLYSUSPEND
	charger->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 2;
	charger->early_suspend.suspend = smb345_early_suspend;
	charger->early_suspend.resume = smb345_late_resume;
	register_early_suspend(&charger->early_suspend);
#endif

	ret = smb345_inok_irq(charger);
	if (ret) {
		SMB_ERR("Failed in requesting ACOK# pin isr\n");
		goto error;
	}

        if(strcmp(mode, "charger") == 0){
                SMB_NOTICE("COS: wake lock in charger\n");
                wake_lock(&COS_wakelock);
        }

	printk("smb345_probe-\n");
	smb345init = true;

	return 0;
error:
	kfree(charger);
	return ret;
}

static int smb345_remove(struct i2c_client *client)
{
	struct smb345_charger *charger = i2c_get_clientdata(client);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&charger->early_suspend);
#endif
	kfree(charger);
	return 0;
}

void smb345_shutdown(struct i2c_client *client)
{
	mutex_lock(&smb347_shutdown_mutex);
	smb347_shutdown = true;
	printk("smb345_shutdown+\n");
	smb345_config_thermal_suspend();
	kfree(charger);
	charger = NULL;
	printk("smb345_shutdown-\n");
	mutex_unlock(&smb347_shutdown_mutex);
}

static const struct i2c_device_id smb345_id[] = {
	{ "smb347_charger", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, smb345_id);

static struct i2c_driver smb345_i2c_driver = {
	.driver	= {
		.name	= "smb347_charger",
	},
	.probe		= smb345_probe,
	.remove		= smb345_remove,
	.shutdown	= smb345_shutdown,
	.id_table	= smb345_id,
};

static int __init smb345_init(void)
{
	printk("smb345 init\n");
	i2c_add_driver(&smb345_i2c_driver);
	return 0;
};
late_initcall(smb345_init);

static void __exit smb345_exit(void)
{
	printk("smb345 exit\n");
	i2c_del_driver(&smb345_i2c_driver);
}
module_exit(smb345_exit);

MODULE_DESCRIPTION("smb345 Battery-Charger");
MODULE_LICENSE("GPL");
