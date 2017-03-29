#include <linux/rotation.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

int sys_set_rotation(int degree) {
	printk(KERN_DEBUG "set_rotation");
	return 1;
}

int sys_rotlock_read(int degree, int range) {

//	printk(KERN_DEBUG "set_rotation");
		return range;
}

int sys_rotlock_write(int degree, int range) {
//	printk(KERN_DEBUG "set_rotation");
		return range;
}

int sys_rotunlock_read(int degree, int range) {
//	printk(KERN_DEBUG "set_rotation");
		return range;
}

int sys_rotunlock_write(int degree, int range) {
//	printk(KERN_DEBUG "set_rotation");
		return range;
}
