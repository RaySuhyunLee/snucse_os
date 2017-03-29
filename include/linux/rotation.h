#include <linux/types.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>

int isValid(int now, int degree, int range){
	int v = 0;
	int a = (360 + degree + range)%360;
	int b = (360 + degree - range)%360;
	if((a-v)*(b-v) <0) return 1;
	return 0;
}

