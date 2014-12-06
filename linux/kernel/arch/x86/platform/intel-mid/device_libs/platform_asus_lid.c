
#include <linux/kernel.h>
#include <linux/platform_device.h>

static struct platform_device asustek_lid_device = {
    .name      = "asustek_lid",
    .id        = -1,
};

static int __init asus_lid_init(void)
{
    printk("asus lid init \n");
    printk("platform init ret = %d \n",platform_device_register(&asustek_lid_device));
    return 0;
}
module_init(asus_lid_init);
