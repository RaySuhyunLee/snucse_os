#include <uapi/asm-generic/errno-base.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>

kuid_t root_uid = { 0 };

#define IS_ROOT(uid) uid_eq((uid), root_uid)
#define IS_CURR_ROOT() IS_ROOT(current_euid())

/*
 *  * Set the SCHED_WRR weight of process, as identified by 'pid'.
 *   * If 'pid' is 0, set the weight for the calling process.
 *    * System call number 380.
 *     */
int sys_sched_setweight(pid_t pid, int weight) {
	struct task_struct *p;
	
	if (pid == 0) {
		p = get_current();
	} else {
		p = find_task_by_vpid(pid);
		if (!p) {
			return -EINVAL;
		}

		if (!uid_eq(task_uid(p), current_uid()) 
			&& !IS_CURR_ROOT()) {
			return -EPERM;
		}
	}

	if (weight > p->wrr.weight && !IS_CURR_ROOT()) {
		return -EPERM;
	}

	//write_lock(p->wrr->weight_lock);
	p->wrr.weight = weight;
	//write_unlock(p->wrr->weight_lock);
	return 0;
}
/*
 *  * Obtain the SCHED_WRR weight of a process as identified by 'pid'.
 *   * If 'pid' is 0, return the weight of the calling process.
 *    * System call number 381.
 *     */
int sys_sched_getweight(pid_t pid){
	struct task_struct *p;
	int weight;

 	printk(KERN_DEBUG "GET");

	if (pid == 0) {
		p = get_current();
	} else {
		p = find_task_by_vpid(pid);
		if (!p) {
			return -EINVAL;
		}
	}

	//read_lock(p->wrr->weight_lock);
	weight = p->wrr.weight;
	//read_unlock(p->wrr->weight_lock);
	return weight;
}
