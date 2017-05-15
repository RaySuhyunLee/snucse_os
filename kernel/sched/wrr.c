#include <uapi/asm-generic/errno-base.h>
#include <linux/slab.h>
#include <linux/irq_work.h>

#include "sched.h"
#include "wrr.h"

#define LOAD_BALANCE_INTERVAL 2*HZ	// FIXME slower than real policy, for TESTING

static unsigned long next_balance;
static raw_spinlock_t balance_lock;

void wrr_set_time_slice(struct sched_wrr_entity* wrr_se) {
	wrr_se->time_slice = msecs_to_jiffies(wrr_se->weight * 10);
}

int wrr_set_weight(struct sched_wrr_entity * entity, int weight) {
	struct task_struct *p;
	struct rq *task_rq;

 	if (weight > WRR_MAX_WEIGHT || weight < WRR_MIN_WEIGHT)
		return -EINVAL;

	write_lock(&entity->weight_lock);
	entity->weight = weight;
	p = container_of(entity, struct task_struct, wrr);
	task_rq = cpu_rq(task_cpu(p));
	if(p != task_rq->curr)
		wrr_set_time_slice(entity);
	write_unlock(&entity->weight_lock);
	return 0;
}

/*
 * initialize sched_wrr class and it's entity(sched_wrr_entity)
 */
void init_sched_wrr_class() {
	struct sched_wrr_entity *wrr_entity;
	wrr_entity = &current->wrr;
	rwlock_init(&wrr_entity->weight_lock);
	wrr_set_weight(wrr_entity, 10);
	
	raw_spin_lock_init(&balance_lock);
	next_balance = jiffies + LOAD_BALANCE_INTERVAL;
}

static void update_curr_wrr(struct rq *rq) {
	struct task_struct *curr = rq->curr;
//	struct sched_wrr_entity *wrr_se = &curr->wrr;
//	struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);
	u64 delta_exec;

//	if(current->pid>5000) printk(KERN_DEBUG "update_curr_wrr\n");

	if (curr->sched_class != &wrr_sched_class)
		return;

	delta_exec = rq->clock_task - curr->se.exec_start;
	if (unlikely((s64)delta_exec < 0))
		delta_exec = 0;

	schedstat_set(curr->se.statistics.exec_max,
		      max(curr->se.statistics.exec_max, delta_exec));

	curr->se.sum_exec_runtime += delta_exec;
	account_group_exec_runtime(curr, delta_exec);

	curr->se.exec_start = rq->clock_task;
	cpuacct_charge(curr, delta_exec);

	//sched_rt_avg_update(rq, delta_exec); //it is for optimizing.
	
	//	would erasing this part be right
//	if (!rt_bandwidth_enabled())
//		return;

/*	for_each_sched_wrr_entity(wrr_se) {
		wrr_rq = wrr_rq_of_se(wrr_se);

		raw_spin_lock(&wrr_rq->wrr_runtime_lock);
		wrr_rq->wrr_time += delta_exec;
		if (sched_wrr_runtime_exceeded(wrr_rq))
			resched_task(curr);
		raw_spin_unlock(&wrr_rq->wrr_runtime_lock);
	}
*/	
}

static inline int on_wrr_rq(struct sched_wrr_entity *wrr_se) {
	if(list_empty(&wrr_se->run_list)) return 0;
	else return 1; 
}

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flag) {
	struct sched_wrr_entity *wrr_se = &p->wrr; //&(p->wrr)
	int weight;

//	if(current->pid>5000) printk(KERN_DEBUG "enqueue_task_wrr\n");
	if(flag & ENQUEUE_WAKEUP) 
		wrr_se->timeout = 0;
	
	wrr_set_time_slice(wrr_se); //initiate time_slice (10 * weight)

	list_add_tail_rcu(&wrr_se->run_list, &rq->wrr.queue);
	++rq -> wrr.wrr_nr_running;
	
	wrr_get_weight(&p->wrr, &weight);
	rq->wrr.total_weight += weight;

	inc_nr_running(rq);
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flag) {
	struct sched_wrr_entity *wrr_se = &p->wrr;
	int weight;
	
//	if(current->pid>5000) printk(KERN_DEBUG "dequeue_task_wrr\n");
	update_curr_wrr(rq);	//I think I have to implement this thing
	list_del_rcu(&wrr_se->run_list);
	--rq -> wrr.wrr_nr_running;
	wrr_get_weight(&p->wrr, &weight);
	rq->wrr.total_weight -= weight;

	dec_nr_running(rq);
}

static void requeue_task_wrr(struct rq *rq, struct task_struct *p) {
	struct wrr_rq *wrr_rq = &rq->wrr;
	struct sched_wrr_entity *wrr_se = &p->wrr;
	wrr_set_time_slice(wrr_se);
//	if(current->pid>5000) printk(KERN_DEBUG "requeue_task_wrr\n");
	list_move_tail(&wrr_se->run_list, &wrr_rq->queue);//rt.c does not lock it
}

static void yield_task_wrr(struct rq *rq) {
	struct sched_wrr_entity *wrr_se = &rq->curr->wrr;

	wrr_set_time_slice(wrr_se);
//	if(current->pid>5000) printk(KERN_DEBUG "yield_task_wrr\n");
	list_move_tail(&wrr_se->run_list, &rq->wrr.queue);
}

static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flag){}

void init_wrr_rq(struct wrr_rq *wrr_rq) {
	//if(current->pid>5000) printk(KERN_DEBUG "init_wrr_rq\n");
	INIT_LIST_HEAD(&wrr_rq->queue);
	wrr_rq->wrr_nr_running = 0;
	wrr_rq->total_weight = 0;
}

static struct task_struct *pick_next_task_wrr(struct rq *rq) {
	struct sched_wrr_entity *wrr_se = NULL;
	struct task_struct *p = NULL;
	struct wrr_rq *wrr_rq = &rq->wrr; //is there a built-in wrr_rq or do we have to do this everytime?
																		//I think I am curently confusing this and totally fixed it. 
	//if(current->pid>5000) printk(KERN_DEBUG "pick_next_task_wrr\n");
	
	if((!wrr_rq->wrr_nr_running)||(list_empty(&wrr_rq->queue)))	return NULL;
	
	wrr_se = list_entry(wrr_rq->queue.next, struct sched_wrr_entity, run_list);
	p = container_of(wrr_se, struct task_struct, wrr);
	if(p==NULL) return NULL; 

	p->se.exec_start = rq->clock_task;
	//the above is said to be done in update_curr So, if it does not work, erase // 
	//Or go and fix update_curr(_wrr)
	
	return p;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *prev) {
	update_curr_wrr(rq);
//	if(current->pid>5000) printk(KERN_DEBUG "put_prev_task_wrr\n");
}

#ifdef CONFIG_SMP
static int select_task_rq_wrr(struct task_struct *p, int sd_flag, int flag) {

	//static int cpu_i = 0;
	int old_cpu = task_cpu(p);
	int new_cpu = old_cpu;
	int iter_cpu; 
	struct rq *rq;
	int min_weight = 0;
	struct task_struct* group_leader;

	//do we need to p->nr_cpus_allowed == 1 check?
	//We do not need to change CPU when sd_flag is SD_BALANCE_EXEC although it is very good chance to balancing. See 2682 in core.c
	//	if(!(sd_flag&SD_BALANCE_WAKE) && !(sd_flag && SD_BALANCE_FORK)) return old_cpu;

	
	#ifdef CONFIG_IMPROVEMENT 			// It is much faster than before !!
	if(unlikely(p->tgid != p-> pid)) { // if process has other thread group heads it would be share cache.
	 	group_leader = find_task_by_vpid(p->tgid);
	 	if(group_leader != NULL) 
			return task_cpu(group_leader); //allocate same CPU. 
	}
	#endif  /* CONFIG_IMPROVEMENT */
	rcu_read_lock();

	rq = cpu_rq(old_cpu);
	min_weight = rq -> wrr.total_weight;		
	preempt_disable();

	for_each_cpu(iter_cpu, cpu_online_mask){
		rq = cpu_rq(iter_cpu);
		
		if (rq->wrr.total_weight < min_weight) {
			min_weight = rq->wrr.total_weight;
			new_cpu = iter_cpu;
		}
	}
	preempt_enable();
	
	rcu_read_unlock();
	return new_cpu; 
}
#endif

static void set_curr_task_wrr(struct rq *rq) {
	struct task_struct *p = rq->curr;
	p->se.exec_start = rq->clock_task;
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued){

	update_curr_wrr(rq);
	
	if(p->policy != SCHED_WRR) return;

	if(--p->wrr.time_slice) return;
	wrr_set_time_slice(&p->wrr);
	

	requeue_task_wrr(rq,p);
	set_tsk_need_resched(p);
	return;
}

#ifdef CONFIG_SMP

static void rq_online_wrr(struct rq *rq)
{
}

static void rq_offline_wrr(struct rq *rq)
{
}

static void switched_from_wrr(struct rq*rq, struct task_struct *p)
{
	if (!p->on_rq)
		return;
}

#endif /* CONFIG_SMP */


static void
prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio) {
}

static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task) {
	int weight;
	
	wrr_get_weight(&task->wrr, &weight);
	return msecs_to_jiffies(10*weight);
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p) {
	struct sched_wrr_entity *wrr_entity = &p->wrr;
	wrr_entity -> task = p;
	wrr_set_weight(wrr_entity, 10);
}

const struct sched_class wrr_sched_class = {
	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
	.yield_task		= yield_task_wrr,	

	.check_preempt_curr	= check_preempt_curr_wrr,

	.pick_next_task		= pick_next_task_wrr,	
	.put_prev_task		= put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,	//ok ->need further implementation with weight

	//.set_cpus_allowed       = set_cpus_allowed_wrr,
	.rq_online              = rq_online_wrr,
	.rq_offline             = rq_offline_wrr,
	//.pre_schedule		= pre_schedule_wrr,
	//.post_schedule		= post_schedule_wrr,
	//.task_woken		= task_woken_wrr,
	.switched_from		= switched_from_wrr,
#endif

	.set_curr_task          = set_curr_task_wrr,
	.task_tick		= task_tick_wrr,

	.get_rr_interval	= get_rr_interval_wrr,

	.prio_changed		= prio_changed_wrr,
	.switched_to		= switched_to_wrr,
};


static void wrr_load_balance(void);
static int get_load(struct wrr_rq *);
static struct task_struct * find_task_to_move(struct rq *, int, int);

/*
 * called periodically by core.c
 */
void wrr_trigger_load_balance(struct rq *rq, int cpu) {
	raw_spin_lock(&balance_lock);
	if (time_after_eq(jiffies, next_balance)) {
		next_balance = jiffies + LOAD_BALANCE_INTERVAL;
		wrr_load_balance();
	}
	raw_spin_unlock(&balance_lock);
}

static void wrr_load_balance(void) {
	struct rq *rq;
	struct wrr_rq *wrr_rq;
	struct task_struct *p;	// task to move to another queue
	int i;
	int min_index = 0, max_index = 0;
	int loads[NR_CPUS];
	int active_cores = 0;	// number of active cores
	
	preempt_disable();
	rcu_read_lock();

	// calculate load of each cpu
	for_each_cpu(i, cpu_active_mask) {
		rq = cpu_rq(i);

		wrr_rq = &rq->wrr;
		printk(KERN_ERR "CPU %d: %d tasks in queue\n", i, wrr_rq->wrr_nr_running);
		if (rq->curr)
			printk(KERN_ERR "task(%d) running\n", rq->curr->pid);
		
		active_cores ++;
		
		loads[i] = get_load(wrr_rq);

		// find minimum/maximum load among cores
		if(loads[min_index] > loads[i]) {
			min_index = i;
		}
		if(loads[max_index] < loads[i]) {
			max_index = i;
		}
	}
	
	printk(KERN_ERR "active cores: %d\n", active_cores);

	if (active_cores <= 1) {	// nothing to balance
		return;
	}
	
	p = find_task_to_move(cpu_rq(max_index), loads[min_index], loads[max_index]);
	
	rcu_read_unlock();	

	if (p) {
		printk(KERN_ERR "Move: cpu %d(%d) -> task pid %d, weight %d -> cpu %d(%d)\n", 
				max_index, cpu_rq(max_index)->wrr.wrr_nr_running, p->pid, p->wrr.weight,
				min_index, cpu_rq(min_index)->wrr.wrr_nr_running);

		wrr_migrate_task(p, max_index, min_index);

		printk(KERN_ERR "After: cpu %d(%d) -> task pid %d, weight %d -> cpu %d(%d)\n", 
				max_index, cpu_rq(max_index)->wrr.wrr_nr_running, p->pid, p->wrr.weight,
				min_index, cpu_rq(min_index)->wrr.wrr_nr_running);
	}

	preempt_enable();
}

static int get_load(struct wrr_rq * wrr_rq) {
	struct sched_wrr_entity *wrr_se;
	int total_weight = 0;

	list_for_each_entry(wrr_se, &wrr_rq->queue, run_list) {
		total_weight += wrr_se->weight;
	}

	return total_weight;
}

/*
 * find the most proper task to move from one cpu to another.
 */
static struct task_struct * find_task_to_move(struct rq *this_rq, int min_load, int max_load) {
	struct sched_wrr_entity *wrr_se;
	struct wrr_rq *this_wrr_rq;
	struct task_struct * p;
	struct sched_wrr_entity *moving_entity = NULL;
	int moving_entity_weight = 0;

	this_wrr_rq = &this_rq -> wrr;

	list_for_each_entry(wrr_se, &this_wrr_rq->queue, run_list) {
		p = container_of(wrr_se, struct task_struct, wrr);
		//read_lock(&wrr_se->weight_lock);
		if (p != this_rq->curr		// not currently running
			&&wrr_se->weight > moving_entity_weight
			&& min_load + wrr_se->weight < max_load - wrr_se->weight) {
			moving_entity = wrr_se;
			moving_entity_weight = wrr_se->weight;
		}
		//read_unlock(&wrr_se->weight_lock);
	}

	if (!moving_entity)
		p = NULL;
	else
		p = container_of(moving_entity, struct task_struct, wrr);

	return p;
}


#ifdef CONFIG_SCHED_DEBUG
extern void print_wrr_rq(struct seq_file *m, int cpu, struct wrr_rq *wrr_rq);

void print_wrr_stats(struct seq_file *m, int cpu){
 		struct rq* rq = cpu_rq(cpu);
		print_wrr_rq(m,cpu, &rq->wrr);
 }
#endif /*CONFIG_SCHED_DEBUG */
