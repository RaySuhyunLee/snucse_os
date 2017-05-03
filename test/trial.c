#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>

int main (int argc, char* argv[]) {
	int tmp;
	int i;
	char* name = argv[1];
	int upper_bound  = atoi(argv[2]);
	int weight = atoi(argv[3]);
	int c = 1;
	while (c <= upper_bound) {
	
		printf("trial-%s: %d = ", name,c); 
		tmp = c;

		for(i=2;i<=tmp;i++) {
			if(tmp % i == 0) {
				printf("%d",i);
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
		
	return 0;
}
