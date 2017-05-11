#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <time.h>


int main (int argc, char* argv[]) {
	struct timespec before, after;
	clock_gettime(CLOCK_MONOTONIC, &before);
	long result;
	long long tmp, i;
	long long upper_bound = 10000* 3;    // = atoi(argv[2]);
	long long c = 1;
	char buf[100];
	FILE *out_file;

	// weight setting
	if (argc < 2) {
		printf("Usage: ./test_weight.o [weight]\n");
		return 0;
	}

	if (syscall(380, getpid(), atoi(argv[1])) < 0) {
		printf("Weight setting failed.\nCheck your permission and try again.\n");
		return -1;
	}

	int weight = syscall(381, getpid());

	printf("Start process %d with weight %d\n", getpid(), weight);
	sleep(1);

	// start division
	pid_t pid = getpid();
	while (c <= upper_bound) {
	
		printf("trial %ld : %lld = ",(long)pid, c); 
		tmp = c;

		for(i=2;i<=tmp;i++) {
			if(tmp % i == 0) {
				printf("%lld",i);
				tmp = tmp / i;
				if(tmp % i == 0) printf("* ");
				else if(tmp % i != 0) {
					if(tmp > i)
					printf(" * ");
				}
				i = 1;
			}
		}
		c++;
		printf("\n");
	}
	clock_gettime(CLOCK_MONOTONIC, &after);
	result = after.tv_sec - before.tv_sec;

	sprintf(buf, "/root/result/%d.result", getpid());
	out_file = fopen(buf, "w");
	fprintf(out_file, "weight: %d, time: %ld s\n", weight, result);
	fclose(out_file);
	return 0;
}
