/*
 *linux/drivers/modem_control/ap_req_cdump.h
 */

 #include <linux/kernel.h>
 #include <linux/switch.h>
 #include <linux/notifier.h>

 #ifndef _AP_REQ_CDUMP_H
 #define _AP_REQ_CDUMP_H

 void ap_req_cdump_init(void *drv, void *data);
 void ap_req_cdump_exit(void);
 #endif /* _AP_REQ_CDUMP_H */
