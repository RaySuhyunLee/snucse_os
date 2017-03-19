# os-team20

## 자주 쓰는 커맨드

### SDB 사용하는 법
먼저 정상적으로 artic 부팅하고 root로 로그인, 다음 커맨드를 입력.
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

## Register System Call
### 1. Increment the number of System calls
arch/arm/include/asm/unistd.h
``` 
#define __NR_syscalls  (N)
```
to
```
#define __NR_syscalls  (N+4)
```
Total number of system calls must be a multiplication of 4.

### 2. assign system call number
arch/arm/include/asm/unistd.h
add
`#define __NR_myfunc      (__NR_SYSCALL_BASE+ #)`

### 3. make asmlinkage function
include/linux/syscalls.h
`asmlinkage int my_func()  // if no parameter then write 'void' `

### 4. add to system call table
arch/arm/kernel/calls.S
`call(sys_myfunc)`

### 5. Revise Makefile
kernel/Makefile
` obj -y = ...  ptree.o`

### etc.
 in kernel/myfunc.c
 the name of function must be sys_myfunc()




