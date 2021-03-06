/*
 * HX8528 E58 Touchscreen Controller Driver
 * Header file
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#ifndef HX8528_E58_H
#define HX8528_E58_H

/*************IO control setting***************/
#define TOUCH_TP_ON                 1
#define TOUCH_TP_OFF                0
#define TOUCH_EC_ON                 1
#define TOUCH_EC_OFF                0
#define TOUCH_FW_UPGRADE_ON         1
#define TOUCH_FW_UPGRADE_OFF        0
#define TOUCH_WIFI_ON               1
#define TOUCH_WIFI_OFF              0
#define TOUCH_IOC_MAGIC             0xF4
#define TOUCH_IOC_MAXNR             7
#define TOUCH_INIT                  _IOR(TOUCH_IOC_MAGIC,    1,    int)
#define TOUCH_FW_UPDATE_FLAG        _IOR(TOUCH_IOC_MAGIC,    2,    int)
#define TOUCH_FW_UPDATE_PROCESS     _IOR(TOUCH_IOC_MAGIC,    3,    int)
#define TOUCH_TP_FW_check           _IOR(TOUCH_IOC_MAGIC,    4,    int)

#define TOUCH_FW_UPGRADE_SUCCESS    0
#define TOUCH_FW_UPGRADE_FAIL       1
#define TOUCH_FW_UPGRADE_PROCESS    2
#define TOUCH_FW_UPGRADE_INIT       3
/*************IO control setting***************/

#define HIMAX_TS_NAME               "hx8528-e58"
#define TOUCH_SDEV_NAME             "hx8528-e58"

struct himax_i2c_platform_data {
    uint16_t version;
    int abs_x_min;
    int abs_x_max;
    int abs_y_min;
    int abs_y_max;
    int intr_gpio;
    int rst_gpio;
};

#endif /* HX8528_E58_H */
