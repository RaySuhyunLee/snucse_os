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

DEFINE_SPINLOCK(list_lock);

struct task_info {
	int pid;
	struct list_head bounds;
	struct list_head list;
};

struct bound {
	int degree;
	int range;
	struct list_head list;
	int is_locked;
};

//Should these function have locker (&lock);
int put_bound(struct list_head *bounds,int degree, int range) {
	struct bound * newBound = kmalloc(sizeof(struct bound), GFP_KERNEL);
	if(!newBound) return -ENOMEM;
	newBound->degree = degree;
	newBound->range = range;
	newBound->is_locked = 0;
	list_add_tail(&(newBound->list), bounds);
	//printk(KERN_DEBUG "new bound added(%d, %d)\n", newBound->degree, newBound->range);
	return 1; 
}

int put_task(struct list_head *tasks, int pid, int degree, int range) {
	struct task_info *task_buf;
	struct task_info *newTask;
//	printk(KERN_DEBUG "put_task called with pid: %d\n", pid);
	int ret = 0;	
	list_for_each_entry(task_buf, tasks, list) {
		if (task_buf->pid == pid) {
			return put_bound(&(task_buf->bounds), degree, range);
		}
	}
	// entry not found(== new task)
	newTask = kmalloc(sizeof(struct task_info), GFP_KERNEL);
	if(!newTask) return -ENOMEM;
	newTask->pid = pid;
	INIT_LIST_HEAD(&newTask->bounds);
	ret = put_bound(&(newTask->bounds), degree, range);
	list_add_tail(&(newTask->list), tasks);
	return ret;
}

/*
 * if remove succeeds(if task exists) return 1
 * else return 0
 */
int remove_bound(struct list_head *bounds, int degree, int range) {
	struct bound *bound_buf;
	list_for_each_entry(bound_buf, bounds, list) {
		if (bound_buf->degree == degree && bound_buf->range == range) {
			list_del(&bound_buf->list);
			kfree(bound_buf);
			//printk(KERN_DEBUG "bound removed (%d %d)" , degree, range);
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
	list_for_each_entry(task_buf, tasks, list) {
		if (task_buf->pid == pid) {
			flag = 1;
			status &= remove_bound(&task_buf->bounds, degree, range); // If one remove_bound is fail then status should set 0.
			if (list_empty(&task_buf->bounds)) {
				//printk(KERN_DEBUG "task removed(pid: %d)\n", task_buf->pid);
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
	// printk(KERN_DEBUG "start isLockable %d %d %d flag :%d\n", degree, range, target,flag);
	return flag;
}

int isInRange_overlap(int degree, int range, int location) {
	int max, min, flag = 0;
	max = degree+range;
	min = degree-range;

	if(location <= max) {
		if(min <= max) flag = 1;
		else flag = (location <= max-360);
	}
	else flag = ((min+360)<=location);

	return flag;
}

int has_overlap(int degree1, int range1, int degree2, int range2){
	int result = 0;
	int i = 0;
	int deg;
	
	for(i = degree1-range1; i<=degree1+range1; i++) {
		deg = convertDegree(i);
		if(isInRange_overlap(degree2, range2, deg)) {
				result = 1;
				return result;
		}
	}
	return result;
}

DEFINE_SPINLOCK(set_rot_lock);	// mutually exclusive lock for sys_set_rotation()

int sys_set_rotation(int degree) {
	int count = 0;
	struct task_info *task_buf;
	struct bound *bound_buf;
	int flag =0;

	if( degree <0 || degree >= 360) {
			return -EINVAL;
	}

	spin_lock(&set_rot_lock);

	spin_lock(&degree_lock);
	_degree = degree;
	spin_unlock(&degree_lock);
	printk(KERN_DEBUG "****************** ROTATION %d *****************\n", degree);
	printk(KERN_DEBUG "READ: %d | WRITE: %d | OCUPIED: %d \n", read_locked[degree], write_locked[degree], write_occupied[degree]);

	spin_lock(&list_lock);
	// search for every available process
	list_for_each_entry(task_buf, &writer_list, list) {
		printk(KERN_DEBUG "writer task found: pid %d\n", task_buf->pid);
		if(flag == 1) break;//write can lock only one at a time.
		list_for_each_entry(bound_buf, &task_buf->bounds, list) {
		 	if(flag == 1) break;
			if (isInRange(bound_buf->degree, bound_buf->range)
					&& isLockable(bound_buf->degree, bound_buf->range, 1)
					&& bound_buf->is_locked == 0) {
				count++;
				flag = 1;
			}
		}
	}
	if (count == 0) {	// if no writer is locked
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
	}
	spin_unlock(&list_lock);

	// wake up every process for range check. NOTE: this is NOT blocking.
	wake_up_queue();
	spin_unlock(&set_rot_lock);

	printk(KERN_DEBUG "READ: %d | WRITE: %d | OCUPIED: %d \n", read_locked[degree], write_locked[degree], write_occupied[degree]);
	printk(KERN_DEBUG "sys_set_rotation returned with %d\n\n", count);
	return count;
}

int sys_rotlock_read(int degree, int range) {
	DEFINE_WAIT(wait);
	int i,deg;
	//////////////////////////
	struct task_info* task_buf;
	struct bound* bound_buf;
	/////////////////////////
	int ret = 0;
	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;
	
	spin_lock(&list_lock);
	ret = put_task(&reader_list, current->pid, degree, range);
	
	spin_unlock(&list_lock);
	if(ret<0) return -1;// kmalloc error

	printk(KERN_DEBUG "rotlock_read\n");
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

	spin_lock(&list_lock);
	set_bound_locked(&reader_list, current->pid, degree, range);
	spin_unlock(&list_lock);

	spin_lock(&locker);
	spin_lock(&list_lock);
	//Increment the number of locks at each degree.
	//when read lock is locked, traverse the write_list and 
	//if there is an overlapping waiting write_lock,
	//increament 'occupied'
	list_for_each_entry(task_buf,&writer_list, list) {
		list_for_each_entry(bound_buf, &task_buf->bounds, list) {
			if((has_overlap(degree,range, bound_buf->degree, bound_buf->range))&&(bound_buf->is_locked != 1)) {
				if(bound_buf->is_locked == 0) bound_buf->is_locked=2;
				else bound_buf->is_locked += 1;
				for(i = (bound_buf->degree)-(bound_buf->range); i<=(bound_buf->degree)+(bound_buf->range); i++) {   //we need to increase it for the bound of the write lock 
					deg = convertDegree(i);
					write_occupied[deg]++;
				}
			}
		}
	}

	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		read_locked[deg]++;
	}
	printk(KERN_DEBUG "READ LOCKED R %d W %d WO %d\n", read_locked[degree], write_locked[degree], write_occupied[degree]);

	spin_unlock(&list_lock);
	spin_unlock(&locker);
	return 0;
}

int sys_rotlock_write(int degree, int range) {
	int i,deg;
	DEFINE_WAIT(wait);
	/////////////////////
	struct task_info *task_buf;
	struct bound *bound_buf;
	struct task_info *task_buf2;
	struct bound *bound_buf2;
	//////////////////////////
	int ret;
	
	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;
	printk(KERN_DEBUG "rotlock_write\n");

	spin_lock(&list_lock);
	// put task into task_info_list
	ret = put_task(&writer_list, current->pid, degree, range);
	spin_unlock(&list_lock);
	if(ret<0) return -1;

	//currently we are increasing for all write locks asked
	//so we shall traverse the read list and check whether there are read locks
	//that have an overlap with the requested write lock and increament
	//occupied each time we meet a read with an overlap
	spin_lock(&locker);
	spin_lock(&list_lock);
	list_for_each_entry(task_buf,&reader_list, list){
		list_for_each_entry(bound_buf, &task_buf->bounds, list) {
		/////////////////////////////////////////
			if((has_overlap(degree, range, bound_buf->degree, bound_buf->range))&&(bound_buf->is_locked==1)){//need to implement this has_overlap function
				for(i = degree-range; i<= degree+range; i++) {
					deg = convertDegree(i);
					write_occupied[deg]++;
				}
				list_for_each_entry(task_buf2, &writer_list, list) {//
					if(task_buf2->pid == current->pid){
						list_for_each_entry(bound_buf2, &task_buf2->bounds, list){
							if(bound_buf2->degree==degree && bound_buf2->range==range && bound_buf2->is_locked!=1){
								if(bound_buf2->is_locked == 0)bound_buf2->is_locked = 2; //change occupied
								else bound_buf2->is_locked += 1;//change for occupied when it is already occupied
							}
						}
					}
				}
			}
		}
	}
	spin_unlock(&list_lock);
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

	spin_lock(&list_lock);
	set_bound_locked(&writer_list, current->pid, degree, range);
	spin_unlock(&list_lock);

	spin_lock(&locker);
	//Increment the number of locks at each degree.
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		write_locked[deg]++;
		//write_occupied[deg]--; now after change, this is not the case of decreasing occupoed
	}
	 printk(KERN_DEBUG "WRITE LOCKED R %d W %d WO %d\n", read_locked[degree], write_locked[degree], write_occupied[degree]);
	spin_unlock(&locker);
	return 0;
}

int sys_rotunlock_read(int degree, int range) {
	int i,deg;
	DEFINE_WAIT(wait);
	///////////////////////
	struct task_info *task_buf;
	struct bound *bound_buf;
	//////////////////////
	
	printk(KERN_DEBUG "rotunlock_read\n");
	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;
	while(!(isInRange(degree,range) )) {
		prepare_to_wait(&read_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		if(signal_pending(current)) {
			remove_wait_queue(&read_q, &wait);
			return 0;
		}
		finish_wait(&read_q,&wait);
	}

	
	spin_lock(&list_lock);
	// check if degree and range exists for given pid
	if (remove_task(&reader_list, current->pid, degree, range) == 0) {
		spin_unlock(&list_lock);
		return -1;
	}
	spin_unlock(&list_lock);

	spin_lock(&locker);

	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		read_locked[deg]--;
	}

	/////////////////////
	//implement part for decreasing occupation
	//when a read lock is unlocked traverse the writer_list
	//for every write lock that is occupied(>=2) and has an overlap
	//decrease the occupied array by 1 for the range of the write lock
	//and decrease the state information of the waiting write lock by 1 (if it was 2 than to 1)
	//
	spin_lock(&list_lock);
	list_for_each_entry(task_buf, &writer_list, list){
		list_for_each_entry(bound_buf, &task_buf->bounds, list){
			if(has_overlap(degree,range,bound_buf->degree,bound_buf->range) && (bound_buf->is_locked>=2)){
			 	printk(KERN_DEBUG "RU occupied-- R %d %d W %d %d\n",degree, range,bound_buf->degree, bound_buf->range) ;
				for(i=(bound_buf->degree)-(bound_buf->range);i<=(bound_buf->degree)+(bound_buf->range);i++){
					deg=convertDegree(i);
					write_occupied[deg]--;
				}
				if(bound_buf->is_locked == 2) bound_buf->is_locked = 0;
				else bound_buf->is_locked -= 1;
			}
		}
	}

	spin_unlock(&list_lock);
	 printk(KERN_DEBUG "READ UNLOCKED R %d W %d WO %d\n", read_locked[degree], write_locked[degree], write_occupied[degree]);
	spin_unlock(&locker);
	wake_up_queue();
	return 0;
}


int sys_rotunlock_write(int degree, int range) {
	int i,deg;
	DEFINE_WAIT(wait);
	
	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;
	printk(KERN_DEBUG "rotunlock_write\n");
	while(!(isInRange(degree,range))) {
		prepare_to_wait(&read_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		if(signal_pending(current)) {
			remove_wait_queue(&read_q, &wait);
			return 0;
		}
		finish_wait(&read_q,&wait);
	}
	//Increment the number of locks at each degree.
	spin_lock(&list_lock);
	// check if degree and range exists for given pid
	if (remove_task(&writer_list, current->pid, degree, range) == 0) {
		spin_unlock(&list_lock);
		return -1; 
	}
	spin_unlock(&list_lock);

	spin_lock(&locker);
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		write_locked[deg]--;
	}

	/////////////////////
	//implement part for decreasing occupation
	//when a read lock is unlocked traverse the writer_list
	//for every write lock that is occupied(>=2) and has an overlap
	//decrease the occupied array by 1 for the range of the write lock
	//and decrease the state information of the waiting write lock by 1 (if it was 2 than to 1)
	//
/*
	list_for_each_entry(task_buf, &writer_list, list){
		list_for_each_entry(bound_buf, &task_buf->bounds, list){
			if(has_overlap(degree,range,bound_buf->degree,bound_buf->range) && (bound_buf->is_locked>=2)){
				for(i=(bound_buf->degree)-(bound_buf->range);i<=(bound_buf->degree)+(bound_buf->range);i++){
					deg=convertDegree(i);
					write_occupied[deg]--;
				}
				if(bound_buf->is_locked == 2) bound_buf->is_locked = 0;
				else bound_buf->is_locked -= 1;
			}
		}
	}
*/
 	printk(KERN_DEBUG "WRITE UNLOCKED R %d W %d WO %d\n", read_locked[degree], write_locked[degree], write_occupied[degree]);
	spin_unlock(&locker);
	wake_up_queue();
	return 0;
	
}

int remove_bound_exit(struct list_head *bounds, int idx) {
	struct bound* bound_buf;
	struct bound* write_buf;
	struct task_info*  task_buf;
	int i, deg, j, k, del_deg,degree, range;
	
	list_for_each_entry(bound_buf, bounds, list) {
		//printk(KERN_DEBUG "%d %d \n" , bound_buf->degree, bound_buf->range);
		list_del(&bound_buf->list);
	
			if(idx == 0) { //reader
			 	if(bound_buf->is_locked == 1) {
					for(i=(bound_buf->degree)-(bound_buf->range);i<=(bound_buf->degree)+(bound_buf->range);i++){
						deg = convertDegree(i);
						read_locked[deg]--;
					}
					degree = bound_buf->degree;
					range = bound_buf->range;
					list_for_each_entry(task_buf, &writer_list, list){
						list_for_each_entry(write_buf, &task_buf->bounds, list){
							if(has_overlap(degree,range,write_buf->degree,write_buf->range) && (write_buf->is_locked>=2)){
			 					printk(KERN_DEBUG "R Kill R %d %d W %d %d\n",degree, range,bound_buf->degree, bound_buf->range) ;
								for(i=(write_buf->degree)-(write_buf->range);i<=(write_buf->degree)+(write_buf->range);i++){
									deg=convertDegree(i);
									write_occupied[deg]--;
								}
								if(write_buf->is_locked == 2) write_buf->is_locked = 0;
								else write_buf->is_locked -= 1;
								
			 					printk(KERN_DEBUG "W_is_locked %d\n",write_buf->is_locked) ;
							}
						}
					}
				}
			}
			else {
				//////////////////////////////
				//this is for decreasing occupied
				//for the amount of is_locked decrease occupied
				printk(KERN_DEBUG "Status : %d\n", bound_buf->is_locked);
				for(k=bound_buf->is_locked; k>1; k--){
			 		printk(KERN_DEBUG "W Kill%d %d %d\n",bound_buf->is_locked,bound_buf->degree, bound_buf->range) ;
					for(j=bound_buf->degree-bound_buf->range;j<=bound_buf->degree+bound_buf->range;j++) {
						del_deg = convertDegree(j);
						write_occupied[del_deg]--;
					}
				

				}
				if(bound_buf->is_locked ==1) {
					for(j=bound_buf->degree-bound_buf->range;j<=bound_buf->degree+bound_buf->range;j++) {
						del_deg = convertDegree(j);
						write_locked[del_deg]--;
					}
		 		}
					/////////////////////////////////////////////////
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
				printk(KERN_DEBUG "EXIT: REMOVE TASK %d\n",pid);
				kfree(task_buf);
				status = 1;
			}
			break;
		}
	}
	return status;
}

void exit_rotlock (void) {
	spin_lock(&list_lock);
	remove_task_exit(&reader_list, current -> pid,0);
	remove_task_exit(&writer_list, current -> pid,1);
	spin_unlock(&list_lock);
	wake_up_queue();
}

