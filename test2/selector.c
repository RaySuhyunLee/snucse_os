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
	
	while (1) {
	
	//use write_lock
	for_lock = syscall(rotlock_write, 90, 90);
	
	f = fopen("integer", "w");
	fputs(start, f);
	fclose(f);
	
	//use write_unlock
	for_unlock = syscall(rotunlock_write, 90, 90);
	
	arg++;
	}
	
	return 0;
}