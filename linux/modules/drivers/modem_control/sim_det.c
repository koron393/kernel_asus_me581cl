#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/mdm_ctrl_board.h>
#include "mdm_util.h"
#include "sim_det.h"

#define SIM_DELAY HZ/10
#define NAME_SIM_PLUG "ril_sim_plug"

static struct workqueue_struct *sim_det_wq;
static struct delayed_work sim_det_work;
static struct switch_dev sim_switch;

static int sim_plug_state;
static bool is_switch;
static struct mdm_ctrl *drv_data;
static struct mdm_ctrl_cpu_data *mdm_cpu;

extern int mdm_ctrl_configure_gpio(int gpio, int direction, int value, const char *desc);

/* callbacks for switch device */
static ssize_t print_sim_plug_name(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%s\n", NAME_SIM_PLUG);
}

static ssize_t print_sim_plug_state(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%d\n", sim_plug_state);
}

void sim_detect_work(struct work_struct *work)
{
	pr_info(DRVNAME": %s GPIO_SIM1_AP_CD = 0x%x\n", __func__, gpio_get_value(mdm_cpu->gpio_sim1_ap_cd));

	sim_plug_state = gpio_get_value(mdm_cpu->gpio_sim1_ap_cd)? 0 : 1;
	if (is_switch)
		switch_set_state(&sim_switch, sim_plug_state);
}

static irqreturn_t sim1_ap_cd_isr(int irq, void *data)
{
	queue_delayed_work(sim_det_wq, &sim_det_work, SIM_DELAY);
	return IRQ_HANDLED;
}

void sim_detect_init(void *drv, void *data)
{
	int ret = 0;

	drv_data = drv;
	mdm_cpu = data;

	sim_det_wq = create_singlethread_workqueue("sim_det_wq");
	INIT_DELAYED_WORK(&sim_det_work, sim_detect_work);

	/* register switch class */
	sim_switch.name = NAME_SIM_PLUG;
	sim_switch.print_name = print_sim_plug_name;
	sim_switch.print_state = print_sim_plug_state;
	ret = switch_dev_register(&sim_switch);
	if (ret < 0) {
		pr_info (DRVNAME": Could not register switch device, ret = %d\n", ret);
		is_switch = 0;
	} else
		is_switch = 1;

	/* Configure the SIM1_AP_CD gpio */
	if (mdm_cpu->gpio_sim1_ap_cd > 0) {
		ret = mdm_ctrl_configure_gpio(mdm_cpu->gpio_sim1_ap_cd,
				0, 0, "SIM1_AP_CD");
		if (ret) {
			pr_err(DRVNAME": config failed for GPIO%d (SIM1_AP_CD)", mdm_cpu->gpio_sim1_ap_cd);
			gpio_free(mdm_cpu->gpio_sim1_ap_cd);
			mdm_cpu->gpio_sim1_ap_cd = 0;
		} else {
			mdm_cpu->irq_sim1_ap_cd = gpio_to_irq(mdm_cpu->gpio_sim1_ap_cd);
			ret = request_irq(mdm_cpu->irq_sim1_ap_cd, sim1_ap_cd_isr,
					IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND,
					"sim1_ap_cd", mdm_cpu);
			if (ret) {
				pr_err(DRVNAME": IRQ request failed for GPIO%d (SIM1_AP_CD)", mdm_cpu->gpio_sim1_ap_cd);
				mdm_cpu->irq_sim1_ap_cd = -1;
			}
		}
	}

	if (mdm_cpu->gpio_sim1_ap_cd > 0)
		queue_delayed_work(sim_det_wq, &sim_det_work, 0);
}

void sim_detect_exit(void)
{
	pr_info(DRVNAME": %s unregister resources\n", __func__);

	if (mdm_cpu->irq_sim1_ap_cd > 0)
		free_irq(mdm_cpu->irq_sim1_ap_cd, NULL);

	cancel_delayed_work_sync(&sim_det_work);
	destroy_workqueue(sim_det_wq);

	if (is_switch) {
		switch_dev_unregister(&sim_switch);
		is_switch = 0;
	}

	if (mdm_cpu->gpio_sim1_ap_cd > 0)
		gpio_free(mdm_cpu->gpio_sim1_ap_cd);
}

