#include <linux/kernel.h>
#include <linux/prinfo.h>

int sys_ptree(struct prinfo * buf, int *nr) {
	printk(KERN_INFO "Hello, World!\n");
	return 0;
}
