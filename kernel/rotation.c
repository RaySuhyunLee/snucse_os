#include <linux/rotation.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>

int _degree;	// current degree
DEFINE_SPINLOCK(degree_lock);

int sys_set_rotation(int degree) {
	spin_lock(&degree_lock);
	_degree = degree;
	spin_unlock(&degree_lock);

	printk(KERN_DEBUG "set_rotation to %d", _degree);
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
