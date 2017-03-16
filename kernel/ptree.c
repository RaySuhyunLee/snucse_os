#include <linux/prinfo.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

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

struct SearchResult {
	struct prinfo *data;
	int max_size;
	int count;
};

void search_process_preorder(struct task_struct*, int*, struct SearchResult*);
void push_task(struct task_struct*, struct SearchResult*);

int sys_ptree(struct prinfo * buf, int *nr) { // buf = point of proc. data,  nr = # of prinfo entries
	
	int num_to_read;
	int process_count = 0;
	struct SearchResult result;
	struct prinfo *data;
	pid_t init_pid = 1;

	printk(KERN_DEBUG "ptree called");

	if( buf == NULL || nr == NULL) return -EINVAL;
	// EFAULT : if buf or nr are outside the accessible address space.

	printk(KERN_DEBUG "copy from user");
	if(copy_from_user(&num_to_read, nr, sizeof(int)) != 0) return -EFAULT ;

	// initialize variables for traversal
	data = kmalloc(sizeof(struct prinfo)*num_to_read, GFP_KERNEL);
	if (!data) return -1; // FIXME proper error handling
	result.data = data;
	result.max_size = num_to_read;
	result.count = 0;
	
	printk(KERN_DEBUG "tasklist locking...\n");
	// lock untii traversal completes, to prevent data structures from changing
	read_lock(&tasklist_lock);
	printk(KERN_DEBUG "start traversal\n");
	search_process_preorder(&init_task, &process_count, &result);
	read_unlock(&tasklist_lock);
	printk(KERN_DEBUG "tasklist unlocked\n");

	printk(KERN_DEBUG "copy to user\n");
	if(copy_to_user(nr, &(result.count), sizeof(int)) != 0) return -EFAULT; ;
	if(copy_to_user(buf, result.data, (result.count) * sizeof(struct prinfo)) !=0 ) return -EFAULT;

	kfree(data);

	printk(KERN_DEBUG "ptree exiting...\n");

	return process_count;
}


/*
 * recursively search for processes in preorder search.
 */
void search_process_preorder(struct task_struct* task, int* count, struct SearchResult *result) {
	struct task_struct* child;
	struct list_head* child_list;
	
	printk(KERN_DEBUG "search_process_preorder: pid = %lu, comm = %s\n", task->pid, task->comm);

  push_task(task, result);
	(*count)++;
	// recursively search for every child
	printk(KERN_DEBUG "search for every child\n");
	list_for_each(child_list, &task->children) {
		child = list_entry(child_list, struct task_struct, sibling);
		
//		comp = container_of(child_list, struct task_struct, sibling);
		printk(KERN_DEBUG "child == %p, child pid=%lu, comm = %s\n", child, child->pid, child->comm);
		search_process_preorder(child, count, result);
	}
}

void push_task(struct task_struct* task, struct SearchResult* result) {
	int count = result->count;
	struct prinfo *data;
	struct task_struct* child;
//	struct task_struct* sibling;
	struct task_struct* sib; // for the JaeD test

	printk(KERN_DEBUG "push_task called\n");

	if (result->count >= result->max_size) return;

	data = result->data;
/*
	struct list_head* temp;
	list_for_each(temp , &task->children) {
		child = list_entry ( temp, struct task_struct, sibling);
		printk(KERN_DEBUG "\n\n\nJAED %d\n\n\n",child->pid);
		break;
	}
*/
//	sib = list_entry ( &(task->sibling), struct task_struct, sibling);

//	child = list_entry ( &(task->children), struct task_struct, children);

 
	child = container_of(&(task->children), struct task_struct, sibling);
	sib = container_of(&(task->sibling.next), struct task_struct, sibling);

	printk(KERN_DEBUG "\n\nJAED %d %d %d\n\n" , task->pid, child->pid, sib->pid);

	data[count].next_sibling_pid = sib->pid;
	data[count].state = task->state;
	data[count].pid = task->pid;
	data[count].parent_pid = task->parent->pid;
	data[count].first_child_pid = child->pid;
//	data[count].next_sibling_pid = sibling->pid;
	data[count].uid = task->cred->uid;
	strcpy(data[count].comm, task->comm);

	result->count++;
}
