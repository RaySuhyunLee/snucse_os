#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <time.h>


int main (void) {// (int argc, char* argv[]) {
 	clock_t before;
	before = clock();
	double result;
	long long tmp, i;
	long long upper_bound = 10000* 10;    // = atoi(argv[2]);
	long long c = 1;
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
	result = (double)(clock()-before)/CLOCKS_PER_SEC;

	printf("Running Time : %5.2f\n", result);
	return 0;
}
