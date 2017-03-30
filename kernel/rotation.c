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
int convertDegree(int n) {
	if(n < 0) return n + 360;
	if(n >=360) return n - 360;
	return n;
}

int isValid(int now, int degree, int range){
	int v = now; //convert rangea.
	int max = degree + range;
	int min = degree - range;
	if ( v <= max) {
		if( min <= v) return 1; // min-v-max
		else return (v <= max-360);//v-min-max

	}
	else return  (min + 360 <= v);//min-max-v
}


int isZero(int degree,int range,int target) { //target 0 : read, 1 : write
	int i;
	for(i = degree-range; i <= degree+range ; i++) {
		if(target ==1 && write_locked[convertDegree(i)] != 0) return 0;
		else if(target == 0 && read_locked[convertDegree(i)] != 0) return 0;
	}
	return 1;
}

int sys_rotlock_read(int degree, int range) {
	DEFINE_WAIT(wait);
	printk(KERN_DEBUG "BEFORE HOLD\n");
	int i,deg;
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
		deg = convertDegree(i);
		read_locked[deg]++;
	}
	spin_unlock(&locker);
	return 0;
}

int sys_rotlock_write(int degree, int range) {
	printk(KERN_DEBUG "W_BEFORE_HOLD\n");
	DEFINE_WAIT(wait);
	int i,deg;
	while(!(isValid(_degree,degree,range) && isZero(degree, range,1) && isZero(degree,range,0))){
	printk(KERN_DEBUG "W_HOLD\n");
		prepare_to_wait(&write_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&write_q,&wait);
	}
	printk(KERN_DEBUG "W_AFTER HOLD!\n");
	spin_lock(&locker);
	printk(KERN_DEBUG "W_spin_lock\n");
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		write_locked[deg]++;
	}
	spin_unlock(&locker);
	return 0;
}

int sys_rotunlock_read(int degree, int range) {
	DEFINE_WAIT(wait);
	int i,deg;
	while(!(isValid(_degree,degree,range))){
		prepare_to_wait(&read_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&read_q,&wait);
	}
	spin_lock_init(&locker); 
	spin_lock(&locker);
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		read_locked[deg]--;
	}
	spin_unlock(&locker);
	return 0;
}


int sys_rotunlock_write(int degree, int range) {
	DEFINE_WAIT(wait);
	int i,deg;
	while(!(isValid(_degree,degree,range))){
		prepare_to_wait(&write_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&write_q,&wait);
	}
	spin_lock_init(&locker); 
	//Increment the number of locks at each degree.
	spin_lock(&locker);
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		write_locked[deg]--;
	}
	return 0;
}
