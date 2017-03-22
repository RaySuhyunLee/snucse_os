#include <linux/prinfo.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

// TODO please explain what this is
#define TASK_PID_MAX 8388608

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

	printk(KERN_DEBUG "[ptree] running...\n");

	if( buf == NULL || nr == NULL) return -EINVAL;
	
	// EFAULT : if buf or nr are outside the accessible address space.
	if(!access_ok(VERIFY_READ, nr, sizeof(int))) return -EFAULT;
	if(!access_ok(VERIFY_READ, buf, sizeof(struct prinfo) * num_to_read)) return -EFAULT;
	
	if(*nr <1) return -EINVAL; 
	//printk(KERN_DEBUG "copy from user\n");
	if(copy_from_user(&num_to_read, nr, sizeof(int)) != 0) return -EAGAIN ;
	
	
	// initialize variables for traversal
	data = kmalloc(sizeof(struct prinfo)*num_to_read, GFP_KERNEL);
	if (!data) return -EAGAIN;
	result.data = data;
	result.max_size = num_to_read;
	result.count = 0;
	
	//printk(KERN_DEBUG "tasklist locking...\n");
	// lock untii traversal completes, to prevent data structures from changing
	read_lock(&tasklist_lock);
	//printk(KERN_DEBUG "start traversal\n");
	
	search_process_preorder(&init_task, &process_count, &result);
	read_unlock(&tasklist_lock);
	//printk(KERN_DEBUG "tasklist unlocked\n");

	//printk(KERN_DEBUG "copy to user\n");
	if(copy_to_user(nr, &(result.count), sizeof(int)) != 0) return -EAGAIN;
	if(copy_to_user(buf, result.data, (result.count) * sizeof(struct prinfo)) !=0 ) return -EAGAIN;

	kfree(data);

	printk(KERN_DEBUG "[ptree] search complete. Exiting...\n");
	
	//when the number of entries is less then one.
	return process_count;
}




/*
 * recursively search for processes in preorder search.
 */
void search_process_preorder(struct task_struct* task, int* count, struct SearchResult *result) {
	struct task_struct* child;
	struct list_head* child_list;
	
	//printk(KERN_DEBUG "search_process_preorder: pid = %lu, comm = %s\n", task->pid, task->comm);

  	push_task(task, result);
	(*count)++;
	// recursively search for every child
	list_for_each(child_list, &task->children) {
		child = list_entry(child_list, struct task_struct, sibling);
		search_process_preorder(child, count, result);
	}
}


void push_task(struct task_struct* task, struct SearchResult* result) {
	int count = result->count;
	struct prinfo *data;
	struct task_struct *child_first = NULL, *sibling_next = NULL;

	//printk(KERN_DEBUG "push_task called\n");
	if (result->count >= result->max_size) return;

	data = result->data;

	child_first = list_first_entry_or_null(&(task->children), struct task_struct, sibling);
	sibling_next = list_first_entry_or_null(&(task->sibling), struct task_struct, sibling);
	if (sibling_next && sibling_next->pid == TASK_PID_MAX)	sibling_next = NULL;

	data[count].state = task->state;
	data[count].pid = task->pid;
	data[count].parent_pid = task->parent->pid;
	data[count].first_child_pid = (child_first != NULL) ? child_first->pid : 0;
	data[count].next_sibling_pid = (sibling_next != NULL) ? sibling_next->pid : 0;
	data[count].uid = task->cred->uid;
	strcpy(data[count].comm, task->comm);

	result->count++;
}
