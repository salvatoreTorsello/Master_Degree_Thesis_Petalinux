#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>


static int __init test01_init(void) {
    printk(KERN_INFO "Test01 LKM: Hello, World!\n");
    return 0;
    // return platform_driver_register(&test01);
}


static void __exit test01_exit(void) {
    printk(KERN_INFO "Test01 LKM: Goodbye, World!\n");
    // platform_driver_unregister(&test01);
}


module_init(test01_init);
module_exit(test01_exit);


MODULE_DESCRIPTION("LKM test01");
MODULE_AUTHOR("Salvatore Torsello");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");