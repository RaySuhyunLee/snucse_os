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
	char str[15];
	
	while (1) {
	
		//use write_lock
		for_lock = syscall(382, 90, 90);
		
		sprintf(str, "%d", arg);
		f = fopen("integer", "w");
		fputs(str, f);
		fclose(f);

		//use write_unlock
		for_unlock = syscall(385, 90, 90);

		printf("selector: %s", str);
		arg++;
	}
		
	return 0;
}
