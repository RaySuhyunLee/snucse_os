#include <linux/prinfo.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>

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
//
struct SearchResult {
	struct prinfo *data;
	int max_size;
	int count;
};

void search_process_preorder(pid_t, int*, struct SearchResult*);
void push_task(struct task_struct*, struct SearchResult*);

int sys_ptree(struct prinfo * buf, int *nr) { // buf = point of proc. data,  nr = # of prinfo entries
	
	int num_to_read;
	int process_count = 0;
	struct SearchResult result;
	struct prinfo *data = kmalloc(sizeof(struct prinfo)*num_to_read, GFP_KERNEL);
	pid_t init_pid = 1;

	if( buf == NULL || nr == NULL) return EINVAL;
	// EFAULT : if buf or nr are outside the accessible address space.

	memcpy(&num_to_read, nr, sizeof(int));

	// initialize variables for traversal
	if (!data) return -1; // FIXME proper error handling
	result.data = data;
	result.max_size = num_to_read;
	result.count = 0;
	
	// lock untii traversal completes, to prevent data structures from changing
	read_lock(&tasklist_lock);
	search_process_preorder(init_pid, &process_count, &result);
	read_unlock(&tasklist_lock);

	memcpy(nr, &(result.count), sizeof(int));
	memcpy(buf, result.data, (result.count) * sizeof(struct prinfo));

	return process_count;
}


/*
 * recursively search for processes in preorder search.
 */
void search_process_preorder(pid_t pid, int* count, struct SearchResult *result) {
	struct task_struct* task = find_task_by_vpid(pid);
	struct task_struct* child;
	struct list_head* child_list;
  push_task(task, result);
	(*count)++;

	// recursively search for every child
	child_list = &(task->children);
	while(child_list != NULL) {
		child = container_of(child_list, struct task_struct, children);
		search_process_preorder(child->pid, count, result);
		child_list = child_list->next;
	}
}

void push_task(struct task_struct* task, struct SearchResult* result) {
	int count = result->count;

	struct prinfo* data;
	struct task_struct* child;
	struct task_struct* sibling;

	if (result->count >= result->max_size) return;

	data = result->data;
	child = container_of(&(task->children), struct task_struct, children);
	sibling = container_of(&(task->sibling), struct task_struct, sibling);

	data[count].state = task->state;
	data[count].pid = task->pid;
	data[count].parent_pid = task->parent->pid;
	data[count].first_child_pid = child->pid;
	data[count].next_sibling_pid = sibling->pid;
	data[count].uid = task->cred->uid;
	strcpy(data[count].comm, task->comm);

	result->count++;
}
