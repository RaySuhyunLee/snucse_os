#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>



int main (int argc, char* argv[]) {
	int pid = 0;
	int weight = 10;

	if (argc == 2) {
		pid = atoi(argv[1]);
	} else if (argc == 3) {
		pid = atoi(argv[1]);
		weight = atoi(argv[2]);
	}

	printf("testing with pid: %d\n", pid);

	int a = syscall(380,pid,weight);

	printf("sys_sched_setweight returned with %d\n", a);
	return 0;
}
