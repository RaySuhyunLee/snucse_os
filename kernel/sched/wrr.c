#include <uapi/asm-generic/errno-base.h>
#include <linux/slab.h>
#include <linux/irq_work.h>

#include "sched.h"
#include "wrr.h"


int wrr_set_weight(struct sched_wrr_entity * entity, int weight) {
	if (weight > WRR_MAX_WEIGHT || weight < WRR_MIN_WEIGHT)
		return -EINVAL;
	write_lock(&entity->weight_lock);
	entity->weight = weight;
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
}

static void update_curr_wrr(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	//struct sched_rt_entity *rt_se = &curr->rt;
	//struct rt_rq *rt_rq = rt_rq_of_se(rt_se);
	u64 delta_exec;

	if(current->pid>5000) printk(KERN_DEBUG "update_curr_wrr\n");

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

	//sched_rt_avg_update(rq, delta_exec);
	
	/*	would erasing this part be right
	if (!rt_bandwidth_enabled())
		return;

	for_each_sched_rt_entity(rt_se) {
		rt_rq = rt_rq_of_se(rt_se);

		if (sched_rt_runtime(rt_rq) != RUNTIME_INF) {
			raw_spin_lock(&rt_rq->rt_runtime_lock);
			rt_rq->rt_time += delta_exec;
			if (sched_rt_runtime_exceeded(rt_rq))
				resched_task(curr);
			raw_spin_unlock(&rt_rq->rt_runtime_lock);
		}
	}
	*/
}

static inline int on_wrr_rq(struct sched_wrr_entity *wrr_se) {
	if(list_empty(&wrr_se->run_list)) return 0;
	else return 1; 
}

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flag) {
	struct sched_wrr_entity *wrr_se = &p->wrr; //&(p->wrr)

	//if(on_wrr_rq(wrr_se)) return;

	if(current->pid>5000) printk(KERN_DEBUG "enqueue_task_wrr\n");
	if(flag & ENQUEUE_WAKEUP) 
		wrr_se->timeout = 0;

	if(current->pid>5000) printk(KERN_DEBUG "Diablo\n");
	list_add_tail(&wrr_se->run_list, &rq->wrr.queue);
	if(current->pid>5000) printk(KERN_DEBUG "Is\n");
	++rq -> wrr.wrr_nr_running;
	if(current->pid>5000) printk(KERN_DEBUG "God\n");

	inc_nr_running(rq);
	if(current->pid>5000) printk(KERN_DEBUG "Game\n");
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flag) {
	struct sched_wrr_entity *wrr_se = &p->wrr;
	
	if(current->pid>5000) printk(KERN_DEBUG "dequeue_task_wrr\n");
	update_curr_wrr(rq);	//I think I have to implement this thing
	list_del(&wrr_se->run_list);
	--rq -> wrr.wrr_nr_running;

	dec_nr_running(rq);
}

static void requeue_task_wrr(struct rq *rq, struct task_struct *p) {
	struct wrr_rq *wrr_rq = &rq->wrr;
	struct sched_wrr_entity *wrr_se = &p->wrr;

	if(current->pid>5000) printk(KERN_DEBUG "requeue_task_wrr\n");
	list_move_tail(&wrr_se->run_list, &wrr_rq->queue);
}

static void yield_task_wrr(struct rq *rq) {
	struct sched_wrr_entity *wrr_se = &rq->curr->wrr;
	struct wrr_rq *wrr_rq;

	if(current->pid>5000) printk(KERN_DEBUG "yield_task_wrr\n");
	list_move_tail(&wrr_se->run_list, &rq->wrr.queue);
}

static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flag){}

void init_wrr_rq(struct wrr_rq *wrr_rq) {
	//if(current->pid>5000) printk(KERN_DEBUG "init_wrr_rq\n");
	INIT_LIST_HEAD(&wrr_rq->queue);
	wrr_rq->wrr_nr_running = 0;
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
	if(current->pid>5000) printk(KERN_DEBUG "put_prev_task_wrr\n");
}

#ifdef CONFIG_SMP
	static int select_task_rq_wrr(struct task_struct *p, int sd_flag, int flag) {
		return 0;
	}

#endif

static void set_curr_task_wrr(struct rq *rq) {
	struct task_struct *p = rq->curr;
	if(current->pid>5000) printk(KERN_DEBUG "set_curr_task_wrr\n");
	p->se.exec_start = rq->clock_task;
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued){
	if(current->pid>5000) printk(KERN_DEBUG "task_tick_wrr\n");
	update_curr_wrr(rq);
	
	if(p->policy != SCHED_WRR) return;

	if(--p->wrr.time_slice > 0) return;

	p->wrr.time_slice = p->wrr.weight;

	if(p->wrr.run_list.prev != p->wrr.run_list.next) {
		requeue_task_wrr(rq,p);
		set_tsk_need_resched(p);
	}
}

#ifdef CONFIG_SMP

static void rq_online_wrr(struct rq *rq)
{
	if(current->pid>5000) printk(KERN_DEBUG "rq_online_wrr\n");
}

static void rq_offline_wrr(struct rq *rq)
{
	if(current->pid>5000) printk(KERN_DEBUG "rq_offline_wrr\n");
}

static void switched_from_wrr(struct rq*rq, struct task_struct *p)
{
	if(current->pid>5000) printk(KERN_DEBUG "switched_from_wrr\n");
	if (!p->on_rq)
		return;
}

#endif /* CONFIG_SMP */


static void
prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio) {
	if(current->pid>5000) printk(KERN_DEBUG "prio_changed_wrr\n");
}

static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task) {
	if(current->pid>4100) printk(KERN_DEBUG "get_rr_interval %d \n", msecs_to_jiffies(10*task->wrr.weight));
	return msecs_to_jiffies(10*task->wrr.weight);
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p) {
	struct sched_wrr_entity *wrr_entity = &p->wrr;
	if(current->pid>5000) printk(KERN_DEBUG "switched_to_wrr\n");
	wrr_entity -> task = p;

	wrr_entity -> weight = 10;
}

const struct sched_class wrr_sched_class = {
	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,	//ok
	.dequeue_task		= dequeue_task_wrr,	//ok
	.yield_task		= yield_task_wrr,	//ok

	.check_preempt_curr	= check_preempt_curr_wrr,	//ok

	.pick_next_task		= pick_next_task_wrr,	//ok
	.put_prev_task		= put_prev_task_wrr,	//ok

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,	//ok ->need further implementation with weight

	//.set_cpus_allowed       = set_cpus_allowed_wrr,
	.rq_online              = rq_online_wrr,	//essential
	.rq_offline             = rq_offline_wrr,	//essential
	//.pre_schedule		= pre_schedule_wrr,
	//.post_schedule		= post_schedule_wrr,
	//.task_woken		= task_woken_wrr,
	.switched_from		= switched_from_wrr,	//essential
#endif

	.set_curr_task          = set_curr_task_wrr,	//ok ->need further implementation with weight
	.task_tick		= task_tick_wrr, //ok ->need further implementation with weight


	.get_rr_interval	= get_rr_interval_wrr,

	.prio_changed		= prio_changed_wrr,
	.switched_to		= switched_to_wrr,
};
