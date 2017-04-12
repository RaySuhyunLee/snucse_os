# os-team20

## High-Level Design & Implementation
Implement three global variables to indicate the ranges of current locks.
1. 'read_locked[360]' contains the range of current read locks  
2. 'write_locked[360]' contains the range of current write locks
3. 'write_occupied[360]' contains the range of pending write locks. Made for preventing Write lock starvation. 

Maintain 2 Wait_Queue 'read_q' 'write_q' for pending locks

Manage 2 lists 'reader_list' 'writer_list' that contains the information of acquired and pending locks.
//Soo Hyun should explain why we put in all locks rather than just acquired locks

Introduce 2 stuctures 'task_info' and 'bound' that contains the information of requested locks
1. 'task_info' maintains the PID of the process and a list of bounds(range and degree) of locks from the process
2. 'bound' maintains the degree and range of a lock

## System Calls (High Level Design)
We introduced 5 new System calls 'set_rotation' 'rotlock_read' 'rotlock_write' 'rotunlock_read' 'rotunlock_write'

Additional Functions (not system calls)
1.  put_bound - adds a new lock bound (degree and range) to the list from a process
2.  put_task - for a new process, it adds a new task and bound. For an existing process, it adds a new bound. 
3.  remove_bound - removes the bound when unlocked
4.  remove_task - removes the task if there exists no bound under it
5.  isInRange - Returns whether the current degree is whithin a certain range
6.  isLockable - Returns if the acquired lock is lockable

Common Features
1.  Use 'spin_locks'
2.  Modify the global variables - to be explained in the following contents 

System call 'set_rotation'
//need to explain how we get the return value from this system call SOO HYUN

System call 'rotlock_read' 
1.  check if the current asked lock 'isLockable' and the degree 'isInRange'
2.  If both conditions are satisfied, the lock is acquired and modifies the global variables. 
3.  If not, the lock goes into an sleep state.

System call 'rotlock_write' 
1.  check if the current asked lock 'isLockable' and the degree 'isInRange'
2.  If both conditions are satisfied, the lock is acquired and modifies the global variables. 
3.  If not, the lock goes into an sleep state.

System call 'rotunlock_read'

System call 'rotunlock_write'

System call ptree[sys_ptree(`380`)] is implemented using recursive strategy. The algorithm consists of three parts.
1. Start with `init_task`, which is the initial task with pid 0.
2. Given a task, push it into the buffer
3. Repeat `2 and 3` for every child process and halt.

Overall design is depicted in the diagram below.  
![](https://github.com/swsnu/os-team20/blob/master/Proj1%20Diagram.png)

To avoid using global variable and achieve better design, we used our own structure named `SearchResult` to manage prinfo values. It's a basic implementation of array list.
```c
struct SearchResult {
	struct prinfo *data; // pointer to prinfo array
	int max_size;        // length of <data>
	int count;           // actual number of prinfo elements
};
```
## Policies



## Lessons Learned
* 프로젝트는 역시 일찍 하는 것이 좋다.
* 커널패닉은 고통스럽지만 printk()와 함께라면 두렵지 않다.
* 손가락부터 움직여선 안되고 반드시 먼저 생각하고 코딩해야 한다.

## How To Register System Call
### 1. Increment the number of System calls
in file: "arch/arm/include/asm/unistd.h"
``` c
#define __NR_syscalls  (N)
```
to
```c
#define __NR_syscalls  (N+4)
```
Total number of system calls must be a multiplication of 4.


