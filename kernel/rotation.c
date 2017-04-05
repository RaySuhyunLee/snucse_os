#include <linux/rotation.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/list.h>

int _degree;	// current degree
DEFINE_SPINLOCK(degree_lock);

int read_locked[360];
int write_locked[360];
DEFINE_SPINLOCK(locker);

DECLARE_WAIT_QUEUE_HEAD(read_q);
DECLARE_WAIT_QUEUE_HEAD(write_q);

LIST_HEAD(reader_list);
LIST_HEAD(writer_list);

struct task_info {
	int pid;
	struct list_head bounds;
	struct list_head list;
};

struct bound {
	int degree;
	int range;
	struct list_head list;
};

void put_bound(struct list_head *bounds,int degree, int range) {
	struct bound * newBound = kmalloc(sizeof(struct bound), GFP_KERNEL);
	newBound->degree = degree;
	newBound->range = range;
	list_add_tail(&(newBound->list), bounds);
	printk(KERN_DEBUG "new bound added(%d, %d)\n", newBound->degree, newBound->range);
}

void put_task(struct list_head *tasks, int pid, int degree, int range) {
	struct task_info *task_buf;
	struct task_info *newTask;
	printk(KERN_DEBUG "put_task called with pid: %d\n", pid);
	list_for_each_entry(task_buf, tasks, list) {
		if (task_buf->pid == pid) {
			put_bound(&(task_buf->bounds), degree, range);
			return;
		}
	}
	printk(KERN_DEBUG "no task found with pid: %d\n", pid);

	// entry not found(== new task)
	newTask = kmalloc(sizeof(struct task_info), GFP_KERNEL);
	newTask->pid = pid;
	INIT_LIST_HEAD(&newTask->bounds);
	put_bound(&(newTask->bounds), degree, range);
	list_add_tail(&(newTask->list), tasks);
	printk(KERN_DEBUG "new task added(pid: %d)\n", newTask->pid);
}

/*
 * if remove succeeds(if task exists) return 1
 * else return 0
 */
int remove_bound(struct list_head *bounds, int degree, int range) {
	struct bound *bound_buf;
	list_for_each_entry(bound_buf, bounds, list) {
		if (bound_buf->degree == degree && bound_buf->range == range) {
			printk(KERN_DEBUG "bound removed(%d, %d)\n", bound_buf->degree, bound_buf->range);
			list_del(&bound_buf->list);
			kfree(bound_buf);
			return 1;
		}
	}
	return 0;
}

/*
 * if remove succeeds(if task exists) return 1
 * else return 0
 */
int remove_task(struct list_head *tasks, int pid, int degree, int range) {
	struct task_info *task_buf;
	int status = 0;
	list_for_each_entry(task_buf, tasks, list) {
		if (task_buf->pid == pid) {
			remove_bound(&task_buf->bounds, degree, range);

			if (list_empty(&task_buf->bounds)) {
				printk(KERN_DEBUG "task removed(pid: %d)\n", task_buf->pid);
				list_del(&task_buf->list);
				kfree(task_buf);
				status = 1;
			}
			break;
		}
	}
	return status;
}


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

int isInRange(int degree, int range) {
	int max, min, flag = 0;
	max = degree + range;
	min = degree - range;
	spin_lock(&degree_lock);
	if ( _degree <= max) {
		if( min <= _degree)
			flag = 1; // min-degree-max
		else
			flag = (_degree <= max-360); //degree-min-max
	} else  {
		flag = (min + 360 <= _degree); //min-max-degree
	}
	spin_unlock(&degree_lock);

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
	// wait until it meets condition
	while(!(isInRange(degree,range) && isLockable(degree, range, 1))){
		prepare_to_wait(&read_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&read_q,&wait);
	}


	spin_lock(&locker);

	// put task into task_info_list
	put_task(&reader_list, current->pid, degree, range);

	//Increment the number of locks at each degree.
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
		prepare_to_wait(&write_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&write_q,&wait);
	}

	spin_lock(&locker);
	// put task into task_info_list
	put_task(&writer_list, current->pid, degree, range);

	//Increment the number of locks at each degree.
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
	
	spin_lock(&locker);
	// check if degree and range exists for given pid
	if (remove_task(&reader_list, current->pid, degree, range) == 0) {
		spin_unlock(&locker);
		return 0; // TODO proper error handling?
	}

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

	//Increment the number of locks at each degree.
	spin_lock(&locker);
	// check if degree and range exists for given pid
	if (remove_task(&writer_list, current->pid, degree, range) == 0) {
		spin_unlock(&locker);
		return 0; // TODO proper error handling?
	}

	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		write_locked[deg]--;
	}
	spin_unlock(&locker);
	return 0;
}
