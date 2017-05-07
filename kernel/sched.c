#include <uapi/asm-generic/errno-base.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include "./sched/wrr.h"

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

	return wrr_set_weight(&p->wrr, weight);
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

	wrr_get_weight(&p->wrr, weight);
	return weight;
}
