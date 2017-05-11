#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <time.h>


int main (void) {// (int argc, char* argv[]) {
	struct timespec before, after;
	clock_gettime(CLOCK_MONOTONIC, &before);
	long result;
	long long tmp, i;
	long long upper_bound = 10000* 3;    // = atoi(argv[2]);
	long long c = 1;
	char buf[40];
	FILE *out_file;
	while (c <= upper_bound) {
	
		printf("trial : %lld = ", c); 
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
	//result = (double)(clock()-before)/CLOCKS_PER_SEC;

	sprintf(buf, "/root/result/pid_%d.result", getpid());
	out_file = fopen(buf, "w");
	fprintf(out_file, "Running Time : %ld s\n", result);
	fclose(out_file);
	return 0;
}
