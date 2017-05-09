# os-team20 Project 3 : Weighted Round-Robin(WRR) Scheduler

## 1. Registering WRR in core.c
Initial scheduler has two policies, Real Time Scheduler and Fair Scheduler(cfs). We inserted a new scheduler: Weighted Round Robin.
Our scheduler basically follows T.A's policy.

## High-Level Design & Implementation
currently developing...


### About time
we initally set weight as 10.
time_slice share same units of jiffies. So we use msces_to_jiffies().
The time_slice is updated when the task is enqueued or requeued.

### About shced_set_weight
When it is called, it firstly check weight is in possible range (1 to 20).
Next, we lock the entity to prevent access its weight.
Then, check it is current running and, if it is not, change time_slice. Because it is already in the queue and running sched_entity will be requeued or dequeued.



## 2. Investigation


## 3. Improvements

## Lessons Learned
* Most build errors are your eye problems. Read error messages carefully!
* Reading 20k lines of code is nothing. Implementing or fixing it is A THING.
* You can (kind of) program Object Oriented in C. It's terribly gourgeous.
* Pair programming can be a powerful solution when nobody knows what to do.
* There is no "End" in kernel development. In other words, we won't go home in constant time.
