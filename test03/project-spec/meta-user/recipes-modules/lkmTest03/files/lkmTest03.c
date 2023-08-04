// based upon the snippets provided in http://xillybus.com/tutorials/device-tree-zynq-4
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/fs.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");
#define DRIVER_NAME "lkmTest03Driver"


static struct file_operations fops;
static int Major, irq;

static struct of_device_id my_driver_of_match[] = {
    { .compatible = "xlnx,test-peripherals-2.0", },
    {}
};
MODULE_DEVICE_TABLE(of, my_driver_of_match);

// ############################################################################
// isr
// ############################################################################

static irqreturn_t testPeriph_isr(int irq, void* dev_id)		
{      
    printk(KERN_INFO "testPeriph_isr(irq=%d)\n", irq);
    return IRQ_HANDLED;
}

// ############################################################################
// platform driver
// ############################################################################

static int my_driver_probe(struct platform_device* dev)
{
    printk(KERN_INFO "my_driver_probe()\n");

	int status;

    irq = irq_of_parse_and_map(dev->dev.of_node, 0);
    printk(KERN_INFO "found irq = %d in device tree\n", irq);
    
    Major = register_chrdev(0, DRIVER_NAME, &fops);

    if (Major == -1) {
        printk (" Dynamic Major number allocation failed\n");
        return Major;
    }
    printk("Char Dev Major number :%d\n", Major);


	status = request_irq(irq, testPeriph_isr, 0, DRIVER_NAME, &dev->dev);

	if (status) {
		printk(KERN_INFO "request irq error = %d\n", status);
        return -1;
	}

    printk(KERN_INFO "registered irq\n");

    printk("The module is successfully loaded\n");
    printk("Major number for %s:   %d\n", DRIVER_NAME, Major);
    printk("IRQ number for %s:     %d\n", DRIVER_NAME, irq);
    
    return 0;
}

static int my_driver_remove(struct platform_device* dev)
{
    printk(KERN_INFO "my_driver_remove()\n");
    printk("Major number %d  IRQ number %d are released\n", Major, irq);

    free_irq(of_irq_get(dev->dev.of_node, 0), &dev->dev);
    unregister_chrdev(Major, DRIVER_NAME);
    printk("The Module is successfully unloaded\n");

    return 0;
}

static struct platform_driver my_driver = {
    .probe = my_driver_probe,
    .remove = my_driver_remove,
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = my_driver_of_match,
    },
};

// ############################################################################
// module house keeping
// ############################################################################

static int __init my_driver_init(void)  
{
    printk(KERN_INFO "my_driver_init()\n");

    // register platform driver
    if (platform_driver_register(&my_driver)) {                                                     
        printk(KERN_WARNING "failed to register platform driver \"%s\"\n", DRIVER_NAME);
        return -1;                                                
    }
    printk(KERN_INFO "registered platform driver\n");
    
    return 0;
} 

static void __exit my_driver_exit(void)  		
{
    printk(KERN_INFO "my_driver_exit()\n");

    platform_driver_unregister(&my_driver);
}

module_init(my_driver_init);
module_exit(my_driver_exit);
