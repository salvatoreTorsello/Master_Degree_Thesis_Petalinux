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
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/sched/signal.h>


#define DRIVER_NAME "lkmTest04SwitchDriver"

#define GPIO_MAP_SIZE 0xFFFF

#define MYMAJOR 64

/** Global variables and defines for userspace app registration */
#define REGISTER_UAPP _IO('R', 'g')
static struct task_struct *task = NULL;

/* define for Signal sending */
#define SIGNR 44


/* Utilitary functions */
static void setbit_reg32(volatile void __iomem *reg, u32 mask) {
    u32 sw_state = ioread32(reg);
    iowrite32(sw_state | mask, reg);
}

u32 readbit_reg32(volatile void __iomem *reg) {
    u32 sw_state = ioread32(reg);
    return sw_state;
}

static void clearbit_reg32(volatile void __iomem *reg, u32 mask) {
    u32 sw_state = ioread32(reg);
    iowrite32((sw_state & (~mask)), reg);
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
static u32 sw_state;   

/* Driver private data */
struct driver_data {
	dev_t dev_id;
    struct device* dev;
    struct gpio_reg *gpio_register;
	spinlock_t lock;
};



/**
 * @brief This function is called, when the device file is opened
 */
static int my_close(struct inode *device_file, struct file *instance) {
	if(task != NULL)
		task = NULL;
	return 0;
}

/**
 * @brief IOCTL for registering the Userspace app to the kernel module
 */
static long int my_ioctl(struct file *file, unsigned cmd, unsigned long arg) {
	if(cmd == REGISTER_UAPP) {
		task = get_current();
		printk("sw_irq_signal: Userspace app with PID %d is registered\n", task->pid);
	}
	return 0;
}


static struct file_operations fops = {
	.owner = THIS_MODULE,
	.release = my_close,
	.unlocked_ioctl = my_ioctl,
};


static const struct of_device_id gpiosw_of_match[] = {
	{ .compatible = "gpio-ebaz-lkmTest02"},
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, gpiosw_of_match);




/*
** This function is the threaded irq handler
*/
static irqreturn_t sw_thread_fn(int irq, void *dev_id) 
{
	int fd;
	struct siginfo info;

    printk(KERN_INFO "IRQ THREAD\n");

	if((task != NULL) && (sw_state == 0)) {
		memset(&info, 0, sizeof(info));
		info.si_signo = SIGNR;
		info.si_code = SI_QUEUE;

		/* Send the signal */
		if(send_sig_info(SIGNR, (struct kernel_siginfo *) &info, task) < 0) 
			printk("sw_irq_signal: Error sending signal\n");
	}

	return IRQ_HANDLED;
}



/**
 * @brief Interrupt service routine is called, when interrupt is triggered
 */
static irqreturn_t sw_isr(int irq, void* dev_id)		
{   
	struct driver_data *data = (struct driver_data*) dev_id;
    struct gpio_reg *_gpio_register = data->gpio_register;
    printk(KERN_INFO "sw_isr(irq=%d)\n", irq);

	// Read the gpio sw_stateue
	sw_state = readbit_reg32(&_gpio_register->gpio_data);
    printk(KERN_INFO "gpio sw_stateue %d\n", sw_state);

	if(sw_state == 0) {
    	printk(KERN_INFO "Button Pressed!\n", sw_state);
	} else {
    	printk(KERN_INFO "Button Released!\n", sw_state);
	}

	// Reset Interrupt Status Register
	setbit_reg32(&data->gpio_register->ip_isr, 0x1);

    return IRQ_WAKE_THREAD;
}



/**
 * gpiosw_probe - Probe method for the GPIO device.
 * @pdev: pointer to the platform device
 *
 * Return:
 * It returns 0, if the driver is bound to the GPIO device, or
 * a negative sw_stateue if there is an error.
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
    
	ret = devm_request_threaded_irq(dev, irq, sw_isr, sw_thread_fn, (IRQF_TRIGGER_RISING | IRQF_SHARED), DRIVER_NAME, data);
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



	/* Register the character device */
	if(register_chrdev(MYMAJOR, "sw_irq_signal", &fops) < 0) {
		printk("Error!\n Can't register device Number!\n");
		free_irq(irq, &pdev->dev);
	}
	printk("Character Device \"sw_irq_signal\" registered. Magor Number %d\n", MYMAJOR);


	
	
	printk(KERN_INFO "GPIO SW probe: DONE!\n");

    return 0;
}




static int gpiosw_remove(struct platform_device *pdev) {
    struct driver_data *data = (struct driver_data *) platform_get_drvdata(pdev);

	free_irq(irq, &pdev->dev);
	unregister_chrdev(MYMAJOR, "sw_irq_signal");
	
    return 0;
}





static struct platform_driver gpiosw_plat_driver = {
	.probe		= gpiosw_probe,
	.remove		= gpiosw_remove,
	.driver		= {
			.name = "gpio-ebaz-lkmTest04",
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

MODULE_AUTHOR("Salvatore Torsello");
MODULE_DESCRIPTION("Button Switch Driver");
MODULE_LICENSE("GPL");