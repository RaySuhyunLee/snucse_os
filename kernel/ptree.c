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

  struct prinfo *_buf;
	int *_nr;

	memcpy(_buf, buf, sizeof(struct prinfo));
	memcpy(_nr, nr, sizeof(int));

	pid_t init_pid = 1;
	process_count = 0;
	struct prinfo search_result[*_nr];
	
	// lock untii traversal to prevent data structures from changing
	read_lock(&tasklist_lock);
	preorder_search(init_pid);
	read_unlock(&tasklist_lock);

//return # of entries on success
}

/*
 * recursively search for processes in preorder search.
 */

void search_process_preorder(pid_t pid, int* count) {
	struct task_struct* task = find_task_by_vpid(pid);
  push_task(task, parray, max);
	*count++;

	list_head* child_list = &task->children;
	while(child_list != NULL) {
		struct task_struct* child = container_of(child_list, struct task_struct, children);
		search_process_preorder(child->pid, parray, nr, count);
		child_list = child_list->next;
	}
}

void push_task(struct *prinfo search_result, struct task_struct* task) {
	if (count > max)

	struct task_struct* child = container_of(&(task->children), struct task_struct, children);
	struct task_struct* sibling = container_of(&(task->sibling), struct task_struct, sibling);
	search_result[count].state = task->state;
	search_result[count].pid = task->pid;
	search_result[count].parent_pid = task->parent->pid;
	search_result[count].first_child_pid = child->pid;
	search_result[count].next_sibling_pid = sibling->pid;
	search_result[count].uid = task->cred->pid;
	strcpy(temp.comm, task->comm);

	count++;
}
