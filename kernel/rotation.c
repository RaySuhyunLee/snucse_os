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
#include <linux/signal.h>

int _degree;	// current degree
DEFINE_SPINLOCK(degree_lock);

int read_locked[360]= {0,};
int write_locked[360] = {0,};
int write_occupied[360] = {0,};

DEFINE_SPINLOCK(locker);

DECLARE_WAIT_QUEUE_HEAD(read_q);
DECLARE_WAIT_QUEUE_HEAD(write_q);

LIST_HEAD(reader_list);
LIST_HEAD(writer_list);

DEFINE_SPINLOCK(reader_list_lock);
DEFINE_SPINLOCK(writer_list_lock);

struct task_info {
	int pid;
	struct list_head bounds;
	struct list_head list;
};

struct bound {
	int degree;
	int range;
	struct list_head list;
	char is_locked;
};

//Should these function have locker (&lock);
void put_bound(struct list_head *bounds,int degree, int range) {
	struct bound * newBound = kmalloc(sizeof(struct bound), GFP_KERNEL);
	newBound->degree = degree;
	newBound->range = range;
	newBound->is_locked = 0;
	list_add_tail(&(newBound->list), bounds);
	printk(KERN_DEBUG "new bound added(%d, %d)\n", newBound->degree, newBound->range);
}

void put_task(struct list_head *tasks, int pid, int degree, int range) {
	struct task_info *task_buf;
	struct task_info *newTask;
//	printk(KERN_DEBUG "put_task called with pid: %d\n", pid);
	
	list_for_each_entry(task_buf, tasks, list) {
		if (task_buf->pid == pid) {
			put_bound(&(task_buf->bounds), degree, range);
			return;
		}
	}
//	printk(KERN_DEBUG "no task found with pid: %d\n", pid);

	// entry not found(== new task)
	newTask = kmalloc(sizeof(struct task_info), GFP_KERNEL);
	newTask->pid = pid;
	INIT_LIST_HEAD(&newTask->bounds);
	put_bound(&(newTask->bounds), degree, range);
	list_add_tail(&(newTask->list), tasks);
//	printk(KERN_DEBUG "new task added(pid: %d)\n", newTask->pid);
}

/*
 * if remove succeeds(if task exists) return 1
 * else return 0
 */
int remove_bound(struct list_head *bounds, int degree, int range) {
	struct bound *bound_buf;
	list_for_each_entry(bound_buf, bounds, list) {
		if (bound_buf->degree == degree && bound_buf->range == range) {
//			printk(KERN_DEBUG "bound removed(%d, %d)\n", bound_buf->degree, bound_buf->range);
			list_del(&bound_buf->list);
			kfree(bound_buf);
			printk(KERN_DEBUG "Remove %d %d Completed" , degree, range);
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
	int status = 1;
	int flag = 0;
	printk(KERN_DEBUG "Remove Task start : %d %d %d\n", pid , degree, range);
	list_for_each_entry(task_buf, tasks, list) {
		if (task_buf->pid == pid) {
			flag = 1;
			status &= remove_bound(&task_buf->bounds, degree, range); // If one remove_bound is fail then status should set 0.
			if (list_empty(&task_buf->bounds)) {
				printk(KERN_DEBUG "task removed(pid: %d)\n", task_buf->pid);
				list_del(&task_buf->list);
				kfree(task_buf);
			}
			break;
		}
	}
	return status&flag;
}

struct task_info* get_task(struct list_head *tasks, int pid) {
	struct task_info *task_buf;
	list_for_each_entry(task_buf, tasks, list) {
		if (task_buf->pid == pid) {
			return task_buf;
		}
	}
	return NULL;
}

void set_bound_locked(struct list_head *tasks, int pid, int degree, int range) {
	struct task_info *task_buf;
	struct bound *bound_buf;

	task_buf = get_task(tasks, pid);
	if (task_buf != NULL) {
		list_for_each_entry(bound_buf, &task_buf->bounds, list) {
			if (bound_buf->degree == degree && bound_buf->range == range && bound_buf->is_locked == 0) {
				bound_buf->is_locked = 1;
				return;
			}
		}
	}
}

void wake_up_queue(void) {
	wake_up(&write_q);
//	printk(KERN_DEBUG "wake up all write lockers\n");
	wake_up(&read_q);
//	printk(KERN_DEBUG "wake up all read lockers\n");
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
	int deg;
	spin_lock(&locker);
	if(target ==0) {
			spin_lock(&degree_lock);
			if(write_occupied[_degree] >0) {
				spin_unlock(&degree_lock);
				spin_unlock(&locker);
				return 0;
			}
			spin_unlock(&degree_lock);
	}
	
	for(i = degree-range; i <= degree+range ; i++) {
		deg = convertDegree(i);
		if(target ==0) {
			if(write_locked[deg]>0) {
				flag = 0;
				break;
			}
		}
		else { //target = 1, write;
			if(write_locked[deg] + read_locked[deg] >0) {
				flag = 0;
				break;
			}
		}
	}
	spin_unlock(&locker);
	printk(KERN_DEBUG "start isLockable %d %d %d flag :%d\n", degree, range, target,flag);
	return flag;
}

DEFINE_SPINLOCK(set_rot_lock);	// mutually exclusive lock for sys_set_rotation()

int sys_set_rotation(int degree) {
	int count = 0;
	struct task_info *task_buf;
	struct bound *bound_buf;

	if( degree <0 || degree >= 360) {
			return -EINVAL;
	}

	spin_lock(&set_rot_lock);

	spin_lock(&degree_lock);
	_degree = degree;
	spin_unlock(&degree_lock);
	printk(KERN_DEBUG "set_rotation to %d occupied %d\n", _degree,write_occupied[_degree]);

	spin_lock(&writer_list_lock);
	// search for every available process
	list_for_each_entry(task_buf, &writer_list, list) {
		printk(KERN_DEBUG "writer task found: pid %d\n", task_buf->pid);
		list_for_each_entry(bound_buf, &task_buf->bounds, list) {
			if (isInRange(bound_buf->degree, bound_buf->range)
					&& isLockable(bound_buf->degree, bound_buf->range, 1)
					&& bound_buf->is_locked == 0) {
				count++;
				// TODO wakeup
				break;	// only one writer can be locked at a time
			}
		}
	}
	spin_unlock(&writer_list_lock);
	if (count == 0) {	// if no writer is locked
		spin_lock(&reader_list_lock);
		list_for_each_entry(task_buf, &reader_list, list) {
			printk(KERN_DEBUG "reader task found: pid %d\n", task_buf->pid);
			list_for_each_entry(bound_buf, &task_buf->bounds, list) {
				if (isInRange(bound_buf->degree, bound_buf->range)
					&& isLockable(bound_buf->degree, bound_buf->range, 0)
					&& bound_buf->is_locked == 0) {
					count++;
				}
			}
		}
		spin_unlock(&reader_list_lock);
	}

	// wake up every process for range check. NOTE: this is NOT blocking.
	wake_up(&write_q);
	wake_up(&read_q);

	spin_unlock(&set_rot_lock);

	printk(KERN_DEBUG "sys_set_rotation returned with %d\n", count);
	return count;
}

int sys_rotlock_read(int degree, int range) {
	DEFINE_WAIT(wait);
	int i,deg;


	
	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;

	spin_lock(&reader_list_lock);
	// put task into task_info_list
	put_task(&reader_list, current->pid, degree, range);
	spin_unlock(&reader_list_lock);

	//printk(KERN_DEBUG "rotlock_read\n");
	// wait until it meets condition
	while(!(isInRange(degree,range) && isLockable(degree, range, 0))) {
		prepare_to_wait(&read_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		if(signal_pending(current)) {
			remove_wait_queue(&read_q, &wait);
			return 0;
		}
		finish_wait(&read_q,&wait);
	}

	spin_lock(&reader_list_lock);
	set_bound_locked(&reader_list, current->pid, degree, range);
	spin_unlock(&reader_list_lock);

	spin_lock(&locker);
	//Increment the number of locks at each degree.
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		read_locked[deg]++;
	}
	printk(KERN_DEBUG "READ LOCKED R %d W %d WO %d\n", read_locked[degree], write_locked[degree], write_occupied[degree]);

	spin_unlock(&locker);
	return 0;
}

int sys_rotlock_write(int degree, int range) {
	int i,deg;
	DEFINE_WAIT(wait);
	
	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;
	//printk(KERN_DEBUG "rotlock_write\n");

	spin_lock(&writer_list_lock);
	// put task into task_info_list
	put_task(&writer_list, current->pid, degree, range);
	spin_unlock(&writer_list_lock);
	
	spin_lock(&locker);
	for(i = degree-range; i<= degree+range; i++) {
		deg = convertDegree(i);
		write_occupied[deg]++;
	}
	spin_unlock(&locker);

	while(!(isInRange(degree,range) && isLockable(degree, range,1))) {
		prepare_to_wait(&write_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		if(signal_pending(current)) {
			remove_wait_queue(&write_q, &wait);
			return 0;
		}
		finish_wait(&write_q,&wait);
	}

	spin_lock(&writer_list_lock);
	set_bound_locked(&writer_list, current->pid, degree, range);
	spin_unlock(&writer_list_lock);

	spin_lock(&locker);
	//Increment the number of locks at each degree.
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		write_locked[deg]++;
		write_occupied[deg]--;
	}
	printk(KERN_DEBUG "WRITE LOCKED R %d W %d WO %d\n", read_locked[degree], write_locked[degree], write_occupied[degree]);
	spin_unlock(&locker);
	return 0;
}

int sys_rotunlock_read(int degree, int range) {
	int i,deg;
	DEFINE_WAIT(wait);
	
	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;
	//printk(KERN_DEBUG "rotunlock_read\n");
	
	spin_lock(&reader_list_lock);
	// check if degree and range exists for given pid
	if (remove_task(&reader_list, current->pid, degree, range) == 0) {
		spin_unlock(&reader_list_lock);
		return -1;
	}
	spin_unlock(&reader_list_lock);

	spin_lock(&locker);
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		read_locked[deg]--;
	}
	printk(KERN_DEBUG "READ UNLOCKED R %d W %d WO %d\n", read_locked[degree], write_locked[degree], write_occupied[degree]);
	spin_unlock(&locker);
	wake_up_queue();
	return 0;
}


int sys_rotunlock_write(int degree, int range) {
	int i,deg;
	DEFINE_WAIT(wait);


	
	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;
	//printk(KERN_DEBUG "rotunlock_write\n");

	//Increment the number of locks at each degree.
	spin_lock(&writer_list_lock);
	// check if degree and range exists for given pid
	if (remove_task(&writer_list, current->pid, degree, range) == 0) {
		spin_unlock(&writer_list_lock);
		return -1; 
	}
	spin_unlock(&writer_list_lock);

	spin_lock(&locker);
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		write_locked[deg]--;
	}
	printk(KERN_DEBUG "WRITE UNLOCKED R %d W %d WO %d\n", read_locked[degree], write_locked[degree], write_occupied[degree]);
	spin_unlock(&locker);
	wake_up_queue();
	return 0;
	
}

int remove_bound_exit(struct list_head *bounds, int idx) {
	struct bound* bound_buf;
	int i, deg;
	
	list_for_each_entry(bound_buf, bounds, list) {
		printk(KERN_DEBUG "%d %d \n" , bound_buf->degree, bound_buf->range);
		list_del(&bound_buf->list);
	
			if(idx == 0) { //reader
				for(i=(bound_buf->degree)-(bound_buf->range);i<=(bound_buf->degree)+(bound_buf->range);i++){
					deg = convertDegree(i);
					read_locked[deg]--;
				}
			}
			else {
				for(i=(bound_buf->degree)-(bound_buf->range);i<=(bound_buf->degree)+(bound_buf->range);i++){
					deg = convertDegree(i);
					write_locked[deg]--;
				}
			}

			kfree(bound_buf);
			return 1; 
	}
	return 0;
}

int remove_task_exit(struct list_head *tasks, int pid, int rw) {
	struct task_info *task_buf;
	int status = 0;
	list_for_each_entry(task_buf, tasks, list) {
		if (task_buf->pid == pid) {
			while(remove_bound_exit(&task_buf->bounds, rw));
			if (list_empty(&task_buf->bounds)) {
				list_del(&task_buf->list);
				printk(KERN_DEBUG "REMOVE TASK %d\n",pid);
				kfree(task_buf);
				status = 1;
			}
			break;
		}
	}
	return status;
}

void exit_rotlock (void) {
	int i;
	spin_lock(&locker);
	remove_task_exit(&reader_list, current -> pid,0);
	remove_task_exit(&writer_list, current -> pid,1);
	spin_unlock(&locker);
}

