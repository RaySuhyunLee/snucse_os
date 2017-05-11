# os-team20 Project 3 : Weighted Round-Robin(WRR) Scheduler

## 1. Registering WRR in core.c
Initial scheduler has two policies, Real Time Scheduler and Fair Scheduler(cfs). We inserted a new scheduler: Weighted Round Robin.
We modified following functions:
```c
void sched_fork(struct task_struct *);
struct task_struct * pick_next_task(struct rq *);
void rt_mutext_setprio(struct task_struct *, int);
void __setscheduler(struct rq *, struct task_struct *, int, int);
void __init sched_init(void);
int cpu_cgroup_can_attach(struct cgroup *, struct cgroup_taskset *);
```

## High-Level Design & Implementation
Our scheduler basically follows T.A's policy.

### About weight
we initally set weight as 10.
We keep track of the total weight of each run queues.
The total weight of each queues are used for allocating CPUs and setting weights.


### About time_slice
Time_slice shares the same units of jiffies. So we use msecs_to_jiffies().
The time_slice is updated when the task is enqueued or requeued. 
When sched_set_weight modified the weight of a task in the runqueue and the task is not currently running, the time slice of the task must be upadated according to the new weight. 


### About shced_set_weight
When it is called, it firstly check weight is in possible range (1 to 20).
Next, we lock the entity to prevent access its weight.
Then, check it is current running and, if it is not, change time_slice. Because it is already in the queue and running sched_entity will be requeued or dequeued.

### Added Classes and fields
We made three classes,
`wrr_sched_class`   ----- in /kernel/sched/wrr.c
`sched_wrr_entity`  ----- in /include/linux/sched.h
`wrr_rq`            ----- in /kernel/sched/sched.h

sched_wrr_entity is an inheritor of sched_entity, task's status and it has `run_list`, `weight`, and `time_slice`

wrr_rq is an inheritor of sched_rq and has `wrr_nr_running` which is the number of running tasks and `queue` which is queue of sched_wrr_entity.

sched_wrr_class is core of our WRR scheduler. it is inheritor of sched_class. It inherits the needed functions for a sched class.
If we refer to sched_rt_class and sched_fair_class we can find the needed functions for wrr_sched_class. In fact, we do not need to implement all of them. The functions that need implement are,

    .enqueue_task = enqueue_task_wrr

    .dequeue_task = dequeue_task_wrr

    .yield_task = yield_task_wrr

    .check_preempt_curr = check_preempt_curr_wrr

    .pick_next_task = pick_next_task_wrr

    .put_next_task = put_prev_task_wrr

    .select_task_rq = select_task_rq_wrr

    .rq_online = rq_online_wrr

    .rq_offline = rq_offline_wrr

    .switched_from = switched_from_wrr

    .set_curr_task = set_curr_task_wrr

    .task_tick = task_tick_wrr

    .get_rr_interval = get_rr_interval

    .prio_changed = prio_changed_wrr

    .switched_to = switched_to_wrr


## 2. Investigation


## 3. Improvements
### Settings
 We make a new flag (CONFIG_IMPROVEMENT) in /arch/arm/configs/artik10_defconfig. Our improvement is in the CONFIG_IMPROVEMENT condition.
 

### Fork
Not every tasks are independent, there are tasks which have certain relations. We can group threads that share the same memory space as thread group. It would be better to allocate these tasks into one CPU to improve performance. To be specific, when the task_struct forked, it call `select_task_rq_wrr()` in `wake_up_new_task()` function. So we compare 

## Lessons Learned
* Most build errors are due to your eyes. Read error messages carefully!
* Reading 20k lines of code is nothing. Implementing or fixing it is A THING.
* You can (kind of) program Object Oriented in C. It's terribly gourgeous.
* Pair programming can be a powerful solution when nobody exactly knows what to do.
* There is no "End" in kernel development. Thus, we can't go home in constant time.
* The Feynman algorithm actually works.
