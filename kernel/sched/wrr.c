#include "sched.h"

#include <linux/slab.h>
#include <linux/irq_work.h>

#define WRR_TIMESLICE 10


const struct sched_class wrr_sched_class = {
	.next			= &fair_sched_class,	//does not need implement
	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
	.yield_task		= yield_task_wrr,

	.check_preempt_curr	= check_preempt_curr_wrr,

	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,

	.set_cpus_allowed       = set_cpus_allowed_wrr,
	.rq_online              = rq_online_wrr,
	.rq_offline             = rq_offline_wrr,
	.pre_schedule		= pre_schedule_wrr,
	.post_schedule		= post_schedule_wrr,
	.task_woken		= task_woken_wrr,
	.switched_from		= switched_from_wrr,
#endif

	.set_curr_task          = set_curr_task_wrr,
	.task_tick		= task_tick_wrr,

	.get_rr_interval	= get_rr_interval_wrr,

	.prio_changed		= prio_changed_wrr,
	.switched_to		= switched_to_wrr,
};

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flag) {
	struct sched_rt_entity *wrr_se = &p->wrr; //&(p->wrr)

	if(flags & ENQUEUE_WAKEUP) 
		wrr_se->timeout = 0;

	list_add_tail(&wrr_se->run_list, &rq->wrr.queue);

	inc_nr_running(rq);
}
