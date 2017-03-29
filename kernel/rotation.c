#include <linux/rotation.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

int _degree;	// current degree
DEFINE_SPINLOCK(degree_lock);

int read_locked[360];
int write_locked[360];
DEFINE_SPINLOCK(locker);

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

int isValid(int now, int degree, int range){
	int v = 0;
	int a = (360 + degree + range)%360;
	int b = (360 + degree - range)%360;
	if((a-v)*(b-v) <0) return 1;
	return 0;
}

int isZero(int degree,int range,int target) { //target 0 : read, 1 : write
	int i;
	for(i = degree-range; i < degree+range ; i++) {
		if(target ==1 && write_locked[1] != 0) return 0;
		else if(read_locked[1] != 0) return 0;
	}
	return 1;
}
int sys_rotlock_read(int degree, int range) {
	DEFINE_WAIT(wait);
	printk(KERN_DEBUG "BEFORE HOLD\n");
	int i;
	while(!(isValid(_degree,degree,range) && isZero(degree, range,1))){

	printk(KERN_DEBUG "HOLD\n");
		prepare_to_wait(&read_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&read_q,&wait);
	}

	printk(KERN_DEBUG "AFTER HOLD\n");
	//Increment the number of locks at each degree.
	spin_lock(&locker);

	printk(KERN_DEBUG "spin_lock\n");
	for(i = degree-range ; i <= degree+range ; i++) {
		if( i <0) read_locked[i+360]++;
		else if( i>=360) read_locked[i-360]++;
		else read_locked[i]++;
	}
	spin_unlock(&locker);
	return 0;
}

int sys_rotlock_write(int degree, int range) {
	printk(KERN_DEBUG "W_BEFORE_HOLD\n");
	DEFINE_WAIT(wait);
	int i,j;
	while(!(isValid(_degree,degree,range) && isZero(degree, range,1) && isZero(degree,range,0))){
	printk(KERN_DEBUG "W_HOLD\n");
		prepare_to_wait(&write_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&write_q,&wait);
	}
	printk(KERN_DEBUG "W_AFTER HOLD\n");
	spin_lock(&locker);
	printk(KERN_DEBUG "write_spin_lock\n");
	for(i = degree-range ; i <= degree+range ; i++) {
		if( i <0) write_locked[i+360]++;
		else if( i>=360) write_locked[i-360]++;
		else write_locked[i]++;
	}
	spin_unlock(&locker);
	return 0;
}

int sys_rotunlock_read(int degree, int range) {
	DEFINE_WAIT(wait);
	int i;
	while(!(isValid(_degree,degree,range))){
		prepare_to_wait(&read_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&read_q,&wait);
	}
	spin_lock_init(&locker); 
	spin_lock(&locker);
	for(i = degree-range ; i <= degree+range ; i++) {
		if( i <0) read_locked[i+360]--;
		else if( i>=360) read_locked[i-360]--;
		else read_locked[i]--;
	}
	spin_unlock(&locker);
	return 0;
}


int sys_rotunlock_write(int degree, int range) {
	DEFINE_WAIT(wait);
	int i;
	while(!(isValid(_degree,degree,range))){
		prepare_to_wait(&write_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&write_q,&wait);
	}
	spin_lock_init(&locker); 
	//Increment the number of locks at each degree.
	spin_lock(&locker);
	for(i = degree-range ; i <= degree+range ; i++) {
		if( i <0) write_locked[i+360]--;
		else if( i>=360) write_locked[i-360]--;
		else write_locked[i]--;
	}
	return 0;
}
