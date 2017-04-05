#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

int main () {
	int c;
	c = syscall(381,40,40);
	printf("read locked!!!!\n");
	c = syscall(383,30,30);
	c= syscall (383,40,40);
	printf("read unlocked!!!\n");
	c= syscall (382,200,40);
	printf("write locked!!!\n");
	c= syscall (385,200,50);
	c= syscall (385,200,40);
	printf("write unlocked!!!\n");
	return 0;
}

