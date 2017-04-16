# os-team20

## High-Level Design & Implementation
* We implemented three global variables to indicate the ranges of current locks. This is to check whether read/write lock can be acquired, in _**constant time**_.
  1. `read_locked[360]` contains the range of current read locks  
  2. `write_locked[360]` contains the range of current write locks
  3. `write_occupied[360]` contains the range of pending write locks. Made for preventing Write lock starvation. 

* We used two Wait_Queue `read_q` and `write_q` for read/write pending locks.

* We have two lists `reader_list` and `writer_list` that contains the information of acquired and pending locks. The perpose of lists are:
  1. To predict which process should grab the lock when device angle is changed.
  2. To check if the bound(degree, range) is valid(check if it exists) for given process on a unlock request.

* We introduced 2 stuctures 'task_info' and 'bound' which contains the information of requested locks.
  1. `task_info` maintains the PID of the process and a list of bounds(range and degree) of locks from the process
  2. `bound` maintains the degree and range of a lock

## Functions (High Level Design)
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

### System call `set_rotation`
1. set `_degree`, which is globally defined variable for device angle, to new value.
2. traverse for every pre-requested boundaries(waiting for lock or already locked) and predict how many lockes will be grabed.
3. wakes up whole processes which are waiting for lock. (each process than checks itself if it can grab the lock)

### System call `rotlock_read`
1.  check if the requested lock is 'isLockable=true' and the degree is 'isInRange=true'
2.  If both conditions are satisfied, the lock is acquired and modifies the global variables. 
3.  If not, the lock goes into an sleep state.

**NOTE** For 'rotlock_read', we modify 'write_occupied' when the read lock is acquired. When a read lock is acquired, we traverse the waiting write locks and if there exists an overlap, we increase 'write_occupied' for the boundaries of the overlapping write locks.

### System call `rotlock_write` 
1.  check if the requested lock is 'isLockable=true' and the degree is 'isInRange=true'
2.  If both conditions are satisfied, the lock is acquired and modifies the global variables. 
3.  If not, the lock goes into an sleep state.

**NOTE** For 'rotlock_write', we modify 'write_occupied' when the write lock is waiting. When a write lock is waiting, we traverse the acquired read locks and it there exists an overlap, we increase 'write_occupied' for the boundary of the acquired write lock for each overlapping read locks.

### System call `rotunlock_read`
1.  check if the required bound(degree and range) actually exists
2.  If the lock exists it is removed and global variables are modified. 

### System call `rotunlock_write`
1.  check if the required bound(degree and range) actually exists
2.  If the lock exists it is removed and global variables are modified. 

**NOTE** The difference between 'rotunlock_read' and 'rotunlock_write' is that, for 'rotunlock_read', we have to check if it has an overlap with a waiting write lock. If there is an ovelap, we have to decrease the 'write_occupied' for the boundary of the ovelapping write lock, which is currently waiting.

Below is an abstracted flow chart for system call functions. Diagram for `rotunlock_write` is skipped since it's implementation seems trivial.

![](https://github.com/swsnu/os-team20/blob/proj2/fig_4.PNG)

___

**NOTE** You should never allocate system call to number 384. For some reason, when a system call is allocated with that number, it is continuously called as soon as kernel boots. We assumed that the number is already reserved and is called right after booting. That's why we register rotunlock_write to system call number 385.
Caution : You must write CALL(sys_ni_syscall)	between CALL(sys_rotlock_write) and CALL(sys_rotunlock_write) in arch/arm/kernel/calls.S

## Policies
![](https://github.com/swsnu/os-team20/blob/proj2/fig_1.PNG)
![](https://github.com/swsnu/os-team20/blob/proj2/fig_2.PNG)
![](https://github.com/swsnu/os-team20/blob/proj2/fig_3.PNG)


## Lessons Learned 
* Early start does not guarantee early end.
* 밤을 많이 새면 정신이 아찔해진다.
* printk()를 너무 많이 쓰면 커널 패닉이 발생한다.
* 귀찮더라도 한번에 다 할 생각 하지 말고 기능 하나 짤때마다 빌드하고 테스트하고 넘어가야 한다.
