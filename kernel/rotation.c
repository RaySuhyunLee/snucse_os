#include <linux/rotation.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <uapi/asm-generic/errno-base.h>

int _degree;	// current degree
DEFINE_SPINLOCK(degree_lock);

int read_locked[360];
int write_locked[360];
DEFINE_SPINLOCK(locker);

DECLARE_WAIT_QUEUE_HEAD(read_q);
DECLARE_WAIT_QUEUE_HEAD(write_q);

int sys_set_rotation(int degree) {

	if( degree <0 || degree >= 360) {
			return -EINVAL;
	}

	spin_lock(&degree_lock);
	_degree = degree;
	spin_unlock(&degree_lock);
	printk(KERN_DEBUG "set_rotation to %d\n", _degree);

	wake_up(&write_q);
//	printk(KERN_DEBUG "wake up all write lockers\n");
	wake_up(&read_q);
//	printk(KERN_DEBUG "wake up all read lockers\n");

	return 1;
}

int convertDegree(int n) {
	if(n < 0) return n + 360;
	if(n >=360) return n - 360;
	return n;
}


int isInRange(int degree, int range){
	int max, min, flag = 0;
	max = degree + range;
	min = degree - range;
	spin_lock(&degree_lock);
	if ( _degree <= max) {
		if( min <= _degree)
			flag = 0; // min-degree-max
		else
			flag = (_degree <= max-360); //degree-min-max
	} else  {
		flag = (min + 360 <= _degree); //min-max-degree
	}
	spin_lock(&degree_lock);

	return flag;
}


int isLockable(int degree,int range,int target) { //target 0 : read, 1 : write
	int i;
	int flag = 1;

	for(i = degree-range; i <= degree+range ; i++) {
		if(target ==1 && write_locked[convertDegree(i)] != 0) {
			flag= 0;
			break;
		}
		else if(target == 0 && read_locked[convertDegree(i)] != 0) {		
			flag =0;
			break;
		}
	}
	return flag;
}

int sys_rotlock_read(int degree, int range) {
	DEFINE_WAIT(wait);
	int i,deg;

	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;

	printk(KERN_DEBUG "rotlock_read\n");
	while(!(isInRange(degree,range) && isLockable(degree, range,1))){
//		printk(KERN_DEBUG "HOLD\n");
		prepare_to_wait(&read_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&read_q,&wait);
	}

//	printk(KERN_DEBUG "AFTER HOLD\n");
	//Increment the number of locks at each degree.
	spin_lock(&locker);

//	printk(KERN_DEBUG "spin_lock\n");
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		read_locked[deg]++;
	}
	spin_unlock(&locker);
	return 0;
}

int sys_rotlock_write(int degree, int range) {
	int i,deg;
	DEFINE_WAIT(wait);

	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;
	printk(KERN_DEBUG "rotlock_write\n");

	while(!(isInRange(degree,range) && isLockable(degree, range,1) && isLockable(degree,range,0))){
//	printk(KERN_DEBUG "W_HOLD\n");
		prepare_to_wait(&write_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&write_q,&wait);
	}
//	printk(KERN_DEBUG "W_AFTER HOLD!\n");
	spin_lock(&locker);
//	printk(KERN_DEBUG "W_spin_lock\n");
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		write_locked[deg]++;
	}
	spin_unlock(&locker);
	return 0;
}

int sys_rotunlock_read(int degree, int range) {
	int i,deg;
	DEFINE_WAIT(wait);

	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;
	printk(KERN_DEBUG "rotunlock_read\n");
	while(!(isInRange(degree,range))){
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
	int i,deg;
	DEFINE_WAIT(wait);

	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;
	printk(KERN_DEBUG "rotunlock_write\n");

	while(!(isInRange(degree,range))){
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
	spin_unlock(&locker);
	return 0;
}
