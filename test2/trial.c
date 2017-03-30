#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <linux/rotation.h>

int main (int start) {
	
	FILE *f;
	int for_lock;
	int for_unlock;
	int arg = start;
	int read_int;
	int tmp;
	int i;
	
	while (1) {
	
		//use write_lock
		for_lock = syscall(382, 90, 90);
		
		f = fopen("integer", "a+");
		fscanf(f, "%d", &tmp);
		//fputs(start, f);
		//fclose(f);
		
		for(i=2;i<=tmp;i++) {
			if(tmp % i == 0) {
				fprintf(f,"%d ",i);
				tmp = tmp / i;
				if(tmp % i == 0) fprintf(f,"* ");
				else if(tmp % i != 0) {
					if(tmp > i)
					fprintf(f,"* ");
				}
				i = 1;
			}
		}
		
		//use write_unlock
		for_unlock = syscall(385, 90, 90);
		
		arg++;
	}
		
	return 0;
}
