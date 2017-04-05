#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <linux/rotation.h>

int main (int start, char* argv[]) {
	
	FILE *f;
	int for_lock;
	int for_unlock;
	int arg = start;
	int tmp;
	int i;
	int scan_ret;
	char* str = argv[1];

	while (1) {
	
		//use read_lock
		for_lock = syscall(381, 90, 90);
		
		f = fopen("integer", "r");
		scan_ret = fscanf(f, "%d", &tmp);
		fclose(f);
		printf("trial-%s: %d = ", str,tmp); 
		
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

		printf("\n");
		
		//use read_unlock
		for_unlock = syscall(383, 90, 90);
		
		arg++;
	}
		
	return 0;
}
