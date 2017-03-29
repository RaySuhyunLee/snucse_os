#include <linux/rotation.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
// return 0 : success, -1 : failure

int read_grap[360] = {0};
int write_grap[360]={0};
DECLARE_WAIT_QUEUE_HEAD(q);


int isWriteZero(int degree,int range) {
	int i;
	for(i = degree-range; i < degree+range ; i++) {
		if( write_grap[(i+360)%360] != 0) return 0;
	}
	return 1;
}

int sys_set_rotation(int degree) {

}

int sys_rotlock_read(int degree, int range) {
	
	DEFINE_WAIT(wait);
	int i;
	spinlock_t locker;
//	add_wait_queue(q, &wait);
	while(!(isValid(1,degree,range) && isWriteZero(degree, range))){
		prepare_to_wait(&q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&q,&wait);
	}

	spin_lock_init(&locker); 
	//Increment the number of locks at each degree.
	spin_lock(&locker);
	for(i = degree-range ; i <= degree+range ; i++) read_grap[(i+360)%360]++;
	spin_unlock(&locker);
	
	return 0;
}

int sys_rotlock_write(int degree, int range) {

	int i,j;
	for(i = degree-range ; i <= degree+range ; i++) read_grap[(i+360)%360]++;
	return 0;
}

int sys_rotunlock_read(int degree, int range) {
//	printk(KERN_DEBUG "set_rotation");
	int i,j;
	for(i= degree-range ; i <= degree+range ; i++) {
		if(i< 0) j = i+360;
		else if (i>=360) j = i-360;
		read_grap[j]--;
	}
	return 0;
}

int sys_rotunlock_write(int degree, int range) {
	int i,j;

	for(i= degree-range ; i <= degree+range ; i++) {
		if(i< 0) j = i+360;
		else if (i>=360) j = i-360;
		write_grap[j]--;
	}
	return 0;
}
