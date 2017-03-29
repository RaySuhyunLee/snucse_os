#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

int main () {
	int c;
	c = syscall(381,0,180);
	c= syscall (382,180,180);
	c= syscall (383,0,180);
	c= syscall (385,180,180);
	return 0;
}

