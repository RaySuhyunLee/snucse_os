#include <linux/gps.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/spinlock_types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mount.h>
#include <linux/path.h>
#include <linux/namei.h>
#include <linux/fs.h>
//#include <linux/ext2_fs.h>
#include "../fs/ext2/ext2.h"

struct gps_location __curr_gps_loc = {
	.lat_integer = 39,
	.lat_fractional = 39200,
	.lng_integer = 125,
	.lng_fractional = 762500,
	.accuracy = 0
};

DEFINE_SPINLOCK(gps_lock);

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
		spin_lock(&gps_lock);
		__curr_gps_loc.lat_integer = buf.lat_integer;
		__curr_gps_loc.lat_fractional = buf.lat_fractional;
		__curr_gps_loc.lng_integer = buf.lng_integer;
		__curr_gps_loc.lng_fractional = buf.lng_fractional;
		__curr_gps_loc.accuracy = buf.accuracy; spin_unlock(&gps_lock);
	}

	// TODO remove below when debugging is not needed anymore.
	printk(KERN_DEBUG "Set location to Lat: %d.%d, Lng: %d.%d, Accuracy: %d\n",
			__curr_gps_loc.lat_integer,
			__curr_gps_loc.lat_fractional,
			__curr_gps_loc.lng_integer,
			__curr_gps_loc.lng_fractional,
			__curr_gps_loc.accuracy);
	return 0;
}

/*
 * Copy gps location of given file, into 'loc'.
 */
int sys_get_gps_location (const char __user *pathname, struct gps_location __user *loc) {
	
	unsigned long e;	// to check copy error
	struct inode *inode;
	struct path path;
	struct gps_location* gps;
	char * _pathname;

	int ret;

	if(!access_ok(VERIFY_READ, pathname, sizeof(char)*255)) return -EFAULT;
	if(!access_ok(VERIFY_READ, loc, sizeof(struct gps_location))) return -EFAULT;

	_pathname = kmalloc(sizeof(char)*255, GFP_KERNEL);
	if(!_pathname) return -EAGAIN;
	
	gps = kmalloc(sizeof(struct gps_location), GFP_KERNEL);

	if (copy_from_user(_pathname, pathname, sizeof(char)*255) > 0) {
		return -EINVAL;
	}

	// get inode
	e = kern_path(_pathname, LOOKUP_FOLLOW, &path);
	if (e != 0) {
		return -ENOENT;
	}
	inode = path.dentry->d_inode;

	if (!inode->i_op || !(inode->i_op->permission)) {
		return -EINVAL;
	}
	
	printk(KERN_DEBUG "before permission check\n");
	if(inode->i_op->permission(inode,MAY_READ) == -EACCES) 
		return -EACCES; // Permission Error (authority and locality)

	printk(KERN_DEBUG "before get_gps_location\n");
	if(inode->i_op->get_gps_location){
		ret = inode->i_op->get_gps_location(inode, gps);
	} else 
		return -ENODEV; // no GPS coordinates are embedded in the file
	
	if (ret < 0 ) // some error occured.
		return ret;

	e = copy_to_user(loc, gps, sizeof(struct gps_location));
	if (e > 0) {
		return -EINVAL;
	}
	return 0;
}
