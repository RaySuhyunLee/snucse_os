#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>



int main (int argc, char* argv[]) {
	int pid = 0;
	if (argc == 2) {
		pid = atoi(argv[1]);
	}

	printf("testing with pid: %d\n", pid);

	int b = syscall(381,pid);

	printf("sys_sched_getweight returned with %d\n", b);
	return 0;
}
