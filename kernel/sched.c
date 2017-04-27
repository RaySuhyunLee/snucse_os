#include <uapi/asm-generic/errno-base.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
/*
 *  * Set the SCHED_WRR weight of process, as identified by 'pid'.
 *   * If 'pid' is 0, set the weight for the calling process.
 *    * System call number 380.
 *     */
int sys_sched_setweight(pid_t pid, int weight) {
	printk(KERN_DEBUG "SET");
	return 1;


}
/*
 *  * Obtain the SCHED_WRR weight of a process as identified by 'pid'.
 *   * If 'pid' is 0, return the weight of the calling process.
 *    * System call number 381.
 *     */
int sys_sched_getweight(pid_t pid){
 	printk(KERN_DEBUG "GET");
	return 1;
}
