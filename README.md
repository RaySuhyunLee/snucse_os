# os-team20 Project 3 : Weighted Round-Robin(WRR) Scheduler
## Spec.
Implement WRR scheduler of which policy is 


## 1. Registering WRR in core.c
Initial scheduler has two policies, Real Time Scheduler and Fair Scheduler(cfs). We need to insert between two polices. 

1. The new scheduling policy should serve as the default scheduling policy for swapper and all of its descendants (e.g. systemd and kthread).
2. The base time slice (quantum) should be 10ms. Weights of tasks can range between 1 and 20 (inclusively). A task's time slice is determined by its weight multiplied by the base time slice. The default weight of tasks should be 10 (a 100ms time slice).
3. If the weight of a task currently on a CPU is changed, it should finish its time quantum as it was before the weight change (i.e., increasing the weight of a task currently on a CPU does not extend its current time quantum).
4. When deciding which CPU a task should be assigned to, it should be assigned to the CPU with the smallest total weight (i.e., sum of the weights of the tasks on the CPU's run queue).
5. Periodic load balancing should be implemented such that a single task from the run queue with the highest total weight should be moved to the run queue with the lowest total weight, provided there exists a task in the highest run queue that can be moved to the lowest run queue without causing the lowest run queue's total weight to become greater than or equal to the highest run queue's total weight. The task that should be moved is the highest weighted eligible task which can be moved without causing the weight imbalance to reverse. Tasks that are currently running are not eligible to be moved and some tasks may have restrictions on which CPU they can be run on. Load balancing should be attempted every 2000ms.


## High-Level Design & Implementation




## Lessons Learned
* 프로젝트는 역시 일찍 하는 것이 좋다.
* 커널패닉은 고통스럽지만 printk()와 함께라면 두렵지 않다.
* 손가락부터 움직여선 안되고 반드시 먼저 생각하고 코딩해야 한다.



