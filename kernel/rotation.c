#include <linux/rotation.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
// return 0 : success, -1 : failure


int read_locked[360] = {0};
int write_locked[360] = {0};
DECLARE_WAIT_QUEUE_HEAD(read_q);
DECLARE_WAIT_QUEUE_HEAD(write_q);
spinlock_t locker;

int isZero(int degree,int range,int target) { //target 0 : read, 1 : write
	int i;
	for(i = degree-range; i < degree+range ; i++) {
		if(target ==1 && write_locked[(i+360)%360] != 0) return 0;
		else if(read_locked[(i+360)%360] != 0) return 0;
	}
	return 1;
}

int sys_set_rotation(int degree) {
	return 0;
}

int sys_rotlock_read(int degree, int range) {
	
	DEFINE_WAIT(wait);
	int i;
//	add_wait_queue(q, &wait);
	while(!(isValid(1,degree,range) && isZero(degree, range,1))){
		prepare_to_wait(&read_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&read_q,&wait);
	}

	spin_lock_init(&locker); 
	//Increment the number of locks at each degree.
	spin_lock(&locker);
	for(i = degree-range ; i <= degree+range ; i++) read_locked[(i+360)%360]++;
	spin_unlock(&locker);
	
	return 0;
}

int sys_rotlock_write(int degree, int range) {

	DEFINE_WAIT(wait);
	int i;
	while(!(isValid(1,degree,range) && isZero(degree, range,1) && isZero(degree,range,0))){
		prepare_to_wait(&write_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&write_q,&wait);
	}
	spin_lock_init(&locker); 
	spin_lock(&locker);
	for(i = degree-range ; i <= degree+range ; i++) write_locked[(i+360)%360]++;
	spin_unlock(&locker);
	return 0;
}

int sys_rotunlock_read(int degree, int range) {
	DEFINE_WAIT(wait);
	int i;
	while(!(isValid(1,degree,range))){
		prepare_to_wait(&read_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&read_q,&wait);
	}
	spin_lock_init(&locker); 
	spin_lock(&locker);
	for(i = degree-range ; i <= degree+range ; i++) read_locked[(i+360)%360]--;
	spin_unlock(&locker);
	return 0;
}

int sys_rotunlock_write(int degree, int range) {
	DEFINE_WAIT(wait);
	int i;
	while(!(isValid(1,degree,range))){
		prepare_to_wait(&write_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&write_q,&wait);
	}
	spin_lock_init(&locker); 
	//Increment the number of locks at each degree.
	spin_lock(&locker);
	for(i = degree-range ; i <= degree+range ; i++) write_locked[(i+360)%360]--;
	spin_unlock(&locker);

	return 0;
}
