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
struct SearchResult {
	struct prinfo *data;
	int max_size;
	int count;
}

int ptree(struct prinfo * buf, int *nr) { // buf = point of proc. data,  nr = # of prinfo entries
	if( buf == null || nr == null) return EINVAL;
// EFAULT : if buf or nr are outside the accessible address space.

  struct prinfo *_buf;
	int num_to_read;

	memcpy(&num_to_read, nr, sizeof(int));

	// initialize variables for traversal
	pid_t init_pid = 1;
	process_count = 0;
	struct SearchResult result;
	struct prinfo data[num_to_read];
	result.data = &data;
	result.max_size = num_to_read;
	result.count = 0;
	
	// lock untii traversal completes, to prevent data structures from changing
	read_lock(&tasklist_lock);
	preorder_search(init_pid, &process_count);
	read_unlock(&tasklist_lock);

	memcpy(nr, &result->count, sizeof(int));
	memcpy(buf, result->data, (result->count) * sizeof(struct prinfo));

	return process_count;
}


/*
 * recursively search for processes in preorder search.
 */
void search_process_preorder(pid_t pid, int* count, struct SearchResult *result) {
	struct task_struct* task = find_task_by_vpid(pid);
  push_task(task, result);
	*count++;

	// recursively search for every child
	list_head* child_list = &task->children;
	while(child_list != NULL) {
		struct task_struct* child = container_of(child_list, struct task_struct, children);
		search_process_preorder(child->pid, parray, nr, count);
		child_list = child_list->next;
	}
}

void push_task(struct task_struct* task, struct SearchResult* result) {
	if (result->count >= max) break;

	struct prinfo* data = result->data;
	struct task_struct* child = container_of(&(task->children), struct task_struct, children);
	struct task_struct* sibling = container_of(&(task->sibling), struct task_struct, sibling);
	data[count].state = task->state;
	data[count].pid = task->pid;
	data[count].parent_pid = task->parent->pid;
	data[count].first_child_pid = child->pid;
	data[count].next_sibling_pid = sibling->pid;
	data[count].uid = task->cred->pid;
	strcpy(temp.comm, task->comm);

	result->count++;
}
