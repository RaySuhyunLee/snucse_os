#include <linux/prinfo.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/sched.h>

//depth-first search

/*
struct prinfo {
    long state;              current state of process
    pid_t pid;               process id 
    pid_t parent_pid;        process id of parent 
    pid_t first_child_pid;   pid of oldest child 
    pid_t next_sibling_pid;  pid of younger sibling 
    long uid;                user id of process owner 
    char comm[64];           name of program executed 
};
*/

/* sched.h 
task_struct : 1065th 
read_lock(&tasklist_lock); 
task_lock(struct task_struct *p) defined by sched.h 2268 line
223 line extern rwlock_t tasklist_lock; 

how we define tasklist? is it already defined?
*/

//void read_lock(struct task_struct * p){ task_lock(p);}
//void read_unlock(struct task_struct *p) { task_unlock(p);}

int ptree(struct prinfo * buf, int *nr) { // buf = point of proc. data,  nr = # of prinfo entries
if( buf == null || nr == null) return EINVAL;
// EFAULT : if buf or nr are outside the accessible address space.
//return # of entries on success

}
