# os-team20

## How to Build
Building the kernel is simple. Just type
```sh
$ build
```
on the top directory of this repo.

To build the test code, type
```sh
$ sh test/build.sh test1.c
```
on the top directory. The executable will be located in "test" directory.

## High-Level Design & Implementation
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

## Lessons Learned
* 프로젝트는 역시 일찍 하는 것이 좋다.
* 커널패닉은 고통스럽지만 printk()와 함께라면 두렵지 않다.
* 손가락부터 움직여선 안되고 반드시 먼저 생각하고 코딩해야 한다.

## 자주 쓰는 커맨드

### SDB 사용하는 법
먼저 정상적으로 artik 부팅하고 root로 로그인, 다음 커맨드를 입력.
```sh
direct_set_debug.sh --sdb-set
```

우분투 환경에서 sdb root로 전환 후 push
```sh
sdb root on
push [원본파일] [destination]
```

### printk 출력 레벨 변경
```sh
echo 8 > /proc/sys/kernel/printk
```
이렇게 하면 모든 메시지가 콘솔에 출력됨

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

### 2. assign system call number
in file: "arch/arm/include/uapi/asm/unistd.h"
add
```c
#define __NR_myfunc      (__NR_SYSCALL_BASE+ #) 
```

### 3. make asmlinkage function
in file: "include/linux/syscalls.h"
```c
asmlinkage int my_func()  // if no parameter then write 'void' 
```

### 4. add to system call table
in file: "arch/arm/kernel/calls.S"
```
call(sys_myfunc)
```

### 5. Revise Makefile
in file: "kernel/Makefile"
```
obj -y = ...  ptree.o
```

### etc.
in file: "kernel/myfunc.c"  
the name of function must be sys_myfunc()




