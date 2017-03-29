#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

int main () {
	int c;
	c = syscall(380,3);
	c = syscall(381,1,2);
	c= syscall (382,1,2);
	c= syscall (383,1,2);
	c= syscall (384,1,2);
	return 0;
}

