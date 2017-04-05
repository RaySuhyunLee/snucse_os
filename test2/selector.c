#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <linux/rotation.h>

int main (int argc, char *argv[]) {
	
	FILE *f;
	int for_lock;
	int for_unlock;
	char str[15];
	char * s = argv[1];
	int i =0;
	int arg = 0;
	while(*(s+i)!= NULL) arg = arg*10 + (*(s+(i++))-'0');
	
	while (1) {
	
		//use write_lock
		for_lock = syscall(382, 90, 90);
		
		sprintf(str, "%d", arg);
		f = fopen("integer", "w");
		fputs(str, f);
		fclose(f);

		//use write_unlock
		for_unlock = syscall(385, 90, 90);

		printf("selector: %s \n", str);
		arg++;
	}
		
	return 0;
}
