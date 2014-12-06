#include <linux/gpio.h>
#include <linux/i2c.h>
#include <asm/intel-mid.h>
#include "platform_hpa02254_battery.h"

const struct i2c_board_info hpa02254_board_info = {
};

void *hpa02254_platform_data(void *info)
{
       return NULL;
}
