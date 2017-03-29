#include <linux/rotation.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>
#include <linux/sched.h>

int _degree;	// current degree
DEFINE_SPINLOCK(degree_lock);

DECLARE_WAIT_QUEUE_HEAD(read_q);
DECLARE_WAIT_QUEUE_HEAD(write_q);

int sys_set_rotation(int degree) {
	spin_lock(&degree_lock);
	_degree = degree;
	spin_unlock(&degree_lock);
	printk(KERN_DEBUG "set_rotation to %d\n", _degree);

	wake_up(&write_q);
	printk(KERN_DEBUG "wake up all write lockers\n");
	wake_up(&read_q);
	printk(KERN_DEBUG "wake up all read lockers\n");

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
