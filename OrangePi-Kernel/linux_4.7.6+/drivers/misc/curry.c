#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/sys_config.h>

#include <linux/regulator/consumer.h>

#define DEV_NAME "buddy_test"

#ifdef pppppppp
#endif


/*
 * Get specify pmu voltage.
 */
/*
int _pmu_voltage(const char *name)
{
	struct regulator *regu = NULL;
	int ret;
	int voltage;

	regu = regulator_get(NULL, name);
	if (IS_ERR(regu)) {
		printk("Failt to get regulator %s\n", name);
		return;
	}
	voltage = regulator_get_voltage(regu);


	ret = regulator_enable(regu);
	if (0 != ret) {
		printk("Can't enable regulator\n");
		goto release_regulator;
	}


	ret = regulator_disable(regu);
	if (0 != ret) {
		printk("Can't disable regulator\n");
		goto release_regulator;
	}

release_regulator:
	regulator_put(regu);

	return voltage;
}
*/

/*
 * Get all pmu voltage.
 */
 /*
void pmu_voltage(void)
{
	printk("=====Current Pmu Voltage=====\n");
	printk("dcdc1 %d\n", _pmu_voltage("vcc-nand"));
	printk("dcdc2 %d\n", _pmu_voltage("vdd-cpua"));
	printk("dcdc3 %d\n", _pmu_voltage("vcc-dcdc3"));
	printk("dcdc4 %d\n", _pmu_voltage("vcc-dcdc4"));
	printk("dcdc5 %d\n", _pmu_voltage("vcc-dram"));
	printk("dcdc6 %d\n", _pmu_voltage("vdd-sys"));
	printk("dcdc7 %d\n", _pmu_voltage("vcc-dcdc7"));
	printk("rtc   %d\n", _pmu_voltage("vcc-rtc"));
	printk("aldo1 %d\n", _pmu_voltage("vcc-pe"));
	printk("aldo2 %d\n", _pmu_voltage("vcc-pl"));
	printk("aldo3 %d\n", _pmu_voltage("vcc-pll"));
	printk("dldo1 %d\n", _pmu_voltage("vcc-hdmi-33"));
	printk("dldo2 %d\n", _pmu_voltage("vcc-mipi"));
	printk("dldo3 %d\n", _pmu_voltage("avdd-csi"));
	printk("dldo4 %d\n", _pmu_voltage("vcc-wifi-io"));
	printk("eldo1 %d\n", _pmu_voltage("vcc-pc"));
	printk("eldo2 %d\n", _pmu_voltage("vcc-lcd-0"));
	printk("fldo1 %d\n", _pmu_voltage("vcc-fldo1"));
	printk("fldo2 %d\n", _pmu_voltage("vdd-cpus"));
	printk("gpio0 %d\n", _pmu_voltage("vcc-ctp"));
	printk("gpio1 %d\n", _pmu_voltage("vcc-gpio1"));
	printk("dc1sw %d\n", _pmu_voltage("vcc-lvds"));
}

 * Set regulator voltage.
 */
void set_regulator_voltage(void)
{
	struct regulator *regu = NULL;
	int ret = 0;
	
	regu = regulator_get(NULL, "iovdd-csi");
	if (IS_ERR(regu)) {
		printk("Error: %s some error happen, fail to get regulator\n", __func__);
		return;
	}
	printk("SUCCEED to get regualator handle\n");
	ret = regulator_get_voltage(regu);
	printk("Current voltage is %d\n", ret);

	/* Set output voltage to 3.3v */
	/* Argument: regulator handle, target voltage, Max voltage */
	ret =  regulator_set_voltage(regu, 3300000, 3300000);
	if (ret != 0) {
		printk("%s: Error: some error happen, fail to set regulator voltage!\n", __func__);
		goto release_regulator;
	}
	printk("SUCCEED to set voltage\n");
	ret = regulator_get_voltage(regu);
	printk("Current voltage is %d\n", ret);

	/* Enable regulator */
	ret = regulator_enable(regu);
	if (0 != ret) {
		printk("%s: Error: some error happen, fail to enable regulator\n", __func__);
		goto release_regulator;
	}

	/* Disable regulator */
	ret = regulator_disable(regu);
	if (0 != ret) {
		printk("%s: Error: some error happen, fail to disable regulator\n", __func__);
		goto release_regulator;
	}

release_regulator:
	regulator_put(regu);
}

/* 
 * Get the Wlan gpio information.
 */
static void Wlan_gpio(void)
{
	int ret;
	struct device_node *node;
	const char *string;
	struct gpio_config config;
	unsigned int gpio;

	/* Get device node. */
	node = of_find_node_by_type(NULL, "wlan");
	if (!node) {
		printk("Can't get the node.\n");
	}

	/* Get the string from dts. */
	ret = of_property_read_string(node, "device_type", &string);
	printk("The device type is %s\n", string);
	
	/* Get the gpio information from dts. */
	gpio = of_get_named_gpio_flags(node, "phy_test1", 0, 
			(enum of_gpio_flags *)(unsigned long)&config);
	if (!gpio_is_valid(gpio)) {
		printk(KERN_INFO "Gpio is invalid\n");
		return;
	}
	printk(KERN_INFO "phy_test1 pin=%d\n mul-sel=%d driver %d pull %d data %d gpio %d\n", 
			config.gpio, config.mul_sel, config.drv_level, config.pull,
			config.data, gpio);

	/* Set voltage */
	if (gpio_is_valid(gpio)) {
		printk("GPIO %d is valid\n", gpio);
		gpio_request(gpio, NULL);
		gpio_direction_output(gpio, 1);
		__gpio_set_value(gpio, 1);
	} else
		__gpio_set_value(gpio, 1);

	printk("Current is %d\n", __gpio_get_value(gpio));

}

/*
 * Get the usbc1 gpio information.
 */
static void usbc1_gpio(void)
{
	int ret;
	struct device_node *node;
	const char *string;
	struct gpio_config config;
	unsigned int gpio;

	/* Get device node. */
	node = of_find_node_by_type(NULL, "usbc1");
	if (!node) {
		printk("Can't get the node.\n");
	}

	/* Get the string from dts. */
	ret = of_property_read_string(node, "device_type", &string);
	printk("The device type is %s\n", string);
	
	/* Get the gpio information from dts. */
	gpio = of_get_named_gpio_flags(node, "usb_drv_vbus_gpio", 0, 
			(enum of_gpio_flags *)(unsigned long)&config);
	if (!gpio_is_valid(gpio)) {
		printk(KERN_INFO "Gpio is invalid\n");
		return;
	}
	printk(KERN_INFO "usb_drv_vbus_gpio pin=%d\n mul-sel=%d driver %d pull %d data %d gpio %d\n", 
			config.gpio, config.mul_sel, config.drv_level, config.pull,
			config.data, gpio);
}


static void cpu_gpio(void)
{
	int ret;
	struct device_node *node;
	const char *string;
	struct gpio_config config;
	unsigned int gpio;

	/* Get device node. */
	node = of_find_node_by_type(NULL, "cpu1");
	if (!node) {
		printk("Can't get the node.\n");
	}

	/* Get the string from dts. */
	ret = of_property_read_string(node, "device_type", &string);
	printk("The device type is %s\n", string);
	
}


/*
 * open operation
 */
static int buddy_open(struct inode *inode,struct file *filp)
{
	printk(KERN_INFO "Open device\n");
	cpu_gpio();
	return 0;
}
/*
 * write operation
 */
static ssize_t buddy_write(struct file *filp,const char __user *buf,size_t count,loff_t *offset)
{
	printk(KERN_INFO "Write device\n");
	return 0;
}
/*
  *release operation
  */
static int buddy_release(struct inode *inode,struct file *filp)
{
	printk(KERN_INFO "Release device\n");
	return 0;
}
/*
 * read operation
 */
static ssize_t buddy_read(struct file *filp,char __user *buf,size_t count,
		loff_t *offset)
{
	printk(KERN_INFO "Read device\n");
	return 0;
}
/*
 * file_operations
 */
static struct file_operations buddy_fops = {
	.owner     = THIS_MODULE,
	.open      = buddy_open,
	.release   = buddy_release,
	.write     = buddy_write,
	.read      = buddy_read,
};
/*
 * misc struct 
 */

static struct miscdevice buddy_misc = {
	.minor    = MISC_DYNAMIC_MINOR,
	.name     = DEV_NAME,
	.fops     = &buddy_fops,
};
/*
 * Init module
 */
static __init int buddy_init(void)
{
	misc_register(&buddy_misc);
	printk("buddy_test\n");
	return 0;
}
/*
 * Exit module
 */
static __exit void buddy_exit(void)
{
	printk(KERN_INFO "buddy_exit_module\n");
	misc_deregister(&buddy_misc);
}
/*
 * module information
 */
module_init(buddy_init);
module_exit(buddy_exit);

MODULE_LICENSE("GPL");
