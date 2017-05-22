#include <linux/gps.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

struct gps_location __curr_gps_loc = {
	.lat_integer = 39,
	.lat_fractional = 39200,
	.lng_integer = 125,
	.lng_fractional = 762500,
	.accuracy = 0
};

/*
 * Set current gps location stored in kernel.
 */
int sys_set_gps_location(struct gps_location __user *loc) {
	unsigned long e;	// to check copy error

	struct gps_location buf;

	// first copy to buffer instead of directly copying to __curr_gps_loc
	// because copy_from_user() may fail/
	e = copy_from_user(&buf, loc, sizeof(struct gps_location));

	if (e > 0) {
		return -EINVAL;
	} else {
		// TODO acquire lock
		__curr_gps_loc.lat_integer = buf.lat_integer;
		__curr_gps_loc.lat_fractional = buf.lat_fractional;
		__curr_gps_loc.lng_integer = buf.lng_integer;
		__curr_gps_loc.lng_fractional = buf.lng_fractional;
		__curr_gps_loc.accuracy = buf.accuracy;
		// TODO release lock
	}

	printk(KERN_DEBUG "Set location to Lat: %d.%d, Lng: %d.%d, Accuracy: %d\n",
			__curr_gps_loc.lat_integer,
			__curr_gps_loc.lat_fractional,
			__curr_gps_loc.lng_integer,
			__curr_gps_loc.lng_fractional,
			__curr_gps_loc.accuracy);
	return 0;
}

/*
 * Copy current gps location stored in kernel, into 'loc'.
 */
int sys_get_gps_location (const char __user *pathname, struct gps_location __user *loc) {
	
	unsigned long e;	// to check copy error

	// TODO acquire lock
	e = copy_to_user(loc, &__curr_gps_loc, sizeof(struct gps_location));
	// TODO release lock

	if (e > 0) {
		return -EINVAL;
	}
	return 0;
}
