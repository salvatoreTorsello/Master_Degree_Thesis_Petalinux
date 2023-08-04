#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/gpio/driver.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <asm/io.h>


#define DRIVER_NAME "lkmTest02SwitchDriver"

#define GPIO_MAP_SIZE 0xFFFF



/* Utilitary functions */
static void setbit_reg32(volatile void __iomem *reg, u32 mask) {
    u32 val = ioread32(reg);
    iowrite32(val | mask, reg);
}

u32 readbit_reg32(volatile void __iomem *reg) {
    u32 val = ioread32(reg);
    return val;
}

static void clearbit_reg32(volatile void __iomem *reg, u32 mask) {
    u32 val = ioread32(reg);
    iowrite32((val & (~mask)), reg);
}


typedef u32 volatile reg_t;

#pragma pack(1)
struct gpio_reg {
	reg_t gpio_data;
	reg_t gpio_tri;
	reg_t gpio2_data;
	reg_t gpio2_tri;
	reg_t reserved1[67];
	reg_t gier;
	reg_t ip_isr;
	reg_t reserved2;
	reg_t ip_ier;
};
#pragma pack()

static int irq;

/* Driver private data */
struct driver_data {
	dev_t dev_id;
    struct device* dev;
    struct gpio_reg *gpio_register;
	spinlock_t lock;
};


struct gp_pdrv_genirq_platdata {
	spinlock_t lock;
	unsigned long flags;
	struct platform_device *pdev;
};

static irqreturn_t gpio_isr(int irq, void* dev_id)		
{   
	struct driver_data *data = (struct driver_data*) dev_id;
    struct gpio_reg *_gpio_register = data->gpio_register;
	u32 val;   
    printk(KERN_INFO "gpio_isr(irq=%d)\n", irq);

	// Read the gpio value
	val = readbit_reg32(&_gpio_register->gpio_data);
    printk(KERN_INFO "gpio value %d\n", val);

	if(val == 0) {
    	printk(KERN_INFO "Button Pressed!\n", val);
	} else {
    	printk(KERN_INFO "Button Release!\n", val);
	}

	// Reset Interrupt Status Register
	setbit_reg32(&data->gpio_register->ip_isr, 0x1);

    return IRQ_HANDLED;
}



/**
 * gpiosw_probe - Probe method for the GPIO device.
 * @pdev: pointer to the platform device
 *
 * Return:
 * It returns 0, if the driver is bound to the GPIO device, or
 * a negative value if there is an error.
 */
static int gpiosw_probe(struct platform_device *pdev)
{		
	struct device *dev;
	struct driver_data *data;
    struct resource *res;
    struct resource *region;
	int ret = -EINVAL;

    dev = &pdev->dev;
	data = (struct driver_data*)devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);

    if (data == NULL) {
        return -ENOMEM;
	}

	data->dev = dev;

	spin_lock_init(&data->lock);

	platform_set_drvdata(pdev, (void*)data);


	/* Remap IO region of the device */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if(res == NULL) {
        printk(KERN_ERR "GPIO SW probe: platform_get_resource FAILED.\n");
        dev_err(&pdev->dev, "io region resource not defined");
        return -ENODEV;
    }

    region = devm_request_mem_region(
            dev,
            res->start,
            resource_size(res),
            dev_name(dev)
    );

    if(region == NULL) {
        printk(KERN_ERR "GPIO SW probe: devm_request_mem_region FAILED.\n");
        dev_err(dev, "mem region not requested");
        return -EBUSY;
    }

	// Note: Non cached by default
	data->gpio_register = devm_ioremap(dev, region->start, GPIO_MAP_SIZE);
    if(data->gpio_register <= 0) {
        printk(KERN_ERR "GPIO SW probe: could not remap io region gpio_register.\n");
        dev_err(dev, "could not remap io region");
        return -EFAULT;
    }



	/* Get device's irq number(s) */
    irq = platform_get_irq(pdev, 0);
    if(irq < 0) {
        printk(KERN_ERR "GPIO SW probe: could not get irq number 0\n");
        return -ENXIO;
    }
    printk(KERN_INFO "found irq = %d in device tree\n", irq);
    
	ret = devm_request_irq(dev, irq, gpio_isr, (IRQF_TRIGGER_RISING | IRQF_SHARED), DRIVER_NAME, data);
    if(ret < 0) {
        dev_err(dev, "Could not request irq %d", irq);
        return ret;
    }

	printk("IRQ number for %s:     %d\n", DRIVER_NAME, irq);
    
	
	
	
	/* Initialize the device itself */

	// Set gpio ad input
	setbit_reg32(&data->gpio_register->gpio_tri, 0x1);
	// Enable Global Interrupt 
	setbit_reg32(&data->gpio_register->gier, (0x1 << 31));
	// Enable Interrupt Register
	setbit_reg32(&data->gpio_register->ip_ier, 0x1);
	// Reset Interrupt Status Register
	setbit_reg32(&data->gpio_register->ip_isr, 0x1);

	
	
	printk(KERN_INFO "GPIO SW probe: DONE!\n");

    return 0;
}




static int gpiosw_remove(struct platform_device *pdev) {
    struct driver_data *data = (struct driver_data *) platform_get_drvdata(pdev);

	free_irq(irq, &pdev->dev);

    return 0;
}



static const struct of_device_id gpiosw_of_match[] = {
	{ .compatible = "gpio-ebaz-lkmTest02"},
	{ /* end of list */ },
};

MODULE_DEVICE_TABLE(of, gpiosw_of_match);

static struct platform_driver gpiosw_plat_driver = {
	.probe		= gpiosw_probe,
	.remove		= gpiosw_remove,
	.driver		= {
			.name = "gpio-ebaz-lkmTest02",
			.of_match_table	= gpiosw_of_match,
	},
};

static int __init gpiosw_init(void)
{	
	int status;
	status =  platform_driver_register(&gpiosw_plat_driver);

	printk(KERN_INFO "My GPIO Xilinx init: DONE! Status: %d\n", status);
	
	return status;
}


static void __exit gpiosw_exit(void)
{
	platform_driver_unregister(&gpiosw_plat_driver);
	printk(KERN_INFO "My GPIO Xilinx exit: DONE\n");
}


subsys_initcall(gpiosw_init);
module_exit(gpiosw_exit);

MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION("Xilinx GPIO driver");
MODULE_LICENSE("GPL");