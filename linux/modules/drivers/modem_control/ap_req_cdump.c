#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/mdm_ctrl_board.h>
#include "mdm_util.h"
#include "ap_req_cdump.h"

static struct mdm_ctrl *drv_data;
static struct mdm_ctrl_cpu_data *mdm_cpu;
static struct workqueue_struct *ap_req_cdumo_wq;
static struct delayed_work ap_req_cdump_work;

extern int mdm_ctrl_configure_gpio(int gpio, int direction, int value, const char *desc);

void trigger_cdump_work(struct work_struct *work)
{
	pr_info(DRVNAME": %s\n", __func__);
	gpio_set_value(mdm_cpu->gpio_ap_req_cdump, 1);
	mdelay(1);
	gpio_set_value(mdm_cpu->gpio_ap_req_cdump, 0);
}

void trigger_cdump(void)
{
	queue_delayed_work(ap_req_cdumo_wq, &ap_req_cdump_work, 1*HZ);
}
EXPORT_SYMBOL(trigger_cdump);

static ssize_t req_cdump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "gpio_ap_req_cdump %d\n", (gpio_get_value(mdm_cpu->gpio_ap_req_cdump)? 1 : 0));
}

static ssize_t req_cdump_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t status;
	long value;

	status = strict_strtol(buf, 0, &value);
	pr_info(DRVNAME": %s value = %d\n", __func__, (int)value);

	if (value)
		queue_delayed_work(ap_req_cdumo_wq, &ap_req_cdump_work, 0);

	return size;
}
static DEVICE_ATTR(req_cdump, 0664, req_cdump_show, req_cdump_store);

void ap_req_cdump_init(void *drv, void *data)
{
	int ret = 0;

	drv_data = drv;
	mdm_cpu = data;

	pr_info(DRVNAME": %s\n", __func__);

	ap_req_cdumo_wq = create_singlethread_workqueue("ap_req_cdumo_wq");
	INIT_DELAYED_WORK(&ap_req_cdump_work, trigger_cdump_work);

	/* Configure the AP_REQ_CDUMP gpio */
	if (mdm_cpu->gpio_ap_req_cdump > 0) {
		ret = mdm_ctrl_configure_gpio(mdm_cpu->gpio_ap_req_cdump,
				1, 0, "AP_REQ_CDUMP");
		if (ret) {
			pr_err(DRVNAME": config failed for GPIO%d (AP_REQ_CDUMP)",
					mdm_cpu->gpio_ap_req_cdump);
			gpio_free(mdm_cpu->gpio_ap_req_cdump);
			mdm_cpu->gpio_ap_req_cdump = 0;
		} else {
			ret = device_create_file(drv_data->dev, &dev_attr_req_cdump);
			if (ret)
				pr_info(DRVNAME": fail to create file (req_cdump)\n");
		}
	}
}

void ap_req_cdump_exit(void)
{
	pr_info(DRVNAME": %s\n", __func__);

	device_remove_file(drv_data->dev, &dev_attr_req_cdump);
	cancel_delayed_work_sync(&ap_req_cdump_work);
	destroy_workqueue(ap_req_cdumo_wq);
	if (mdm_cpu->gpio_ap_req_cdump > 0)
		gpio_free(mdm_cpu->gpio_ap_req_cdump);
}
