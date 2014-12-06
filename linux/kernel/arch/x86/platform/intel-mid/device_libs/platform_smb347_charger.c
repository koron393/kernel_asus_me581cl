#include <linux/gpio.h>
#include <linux/i2c.h>
#include <asm/intel-mid.h>
#include "platform_smb347_charger.h"

const struct i2c_board_info smb347_board_info = {
};

void *smb347_platform_data(void *info)
{
	return NULL;
}
