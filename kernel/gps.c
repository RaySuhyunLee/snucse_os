#include<linux/gps.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>



int sys_set_gps_location(struct gps_location __user *loc) {
	printk(KERN_ERR "SET_GPS_LOCATION");

	return 0;

}

int sys_get_gps_location (const char __user *pathname, struct gps_location __user *loc) {
	printk(KERN_ERR "GET_GPS_LOCATION");
	
	return 0;

}
