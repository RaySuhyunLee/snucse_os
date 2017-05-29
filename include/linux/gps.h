#ifndef __GPS_H
#define __GPS_H

#include <linux/types.h>
#include <linux/spinlock_types.h>

struct gps_location {
	int lat_integer;
	int lat_fractional;
	int lng_integer;
	int lng_fractional;
	int accuracy;
};

// global variable that stores current location
extern struct gps_location __curr_gps_loc;

extern spinlock_t gps_lock;
#endif
