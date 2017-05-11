#include<stdio.h>
#include<stdlib.h>
#include<sched.h>
#include<unistd.h>

int main (int argc, char* argv[]) {

	int a = 1;
	// weight setting
	if (argc < 2) {
		printf("Usage: ./test_weight.o [weight]\n");
		return 0;
	}

	if (syscall(380, getpid(), atoi(argv[1])) < 0) {
		printf("Weight setting failed.\nCheck your permission and try again.\n");
		return -1;
	}
	while(a++){
		if(a%100000 == 0 ) {
			printf("a\n");
			a = 1;
		}
	}
	return 0;
}
