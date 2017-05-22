#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<errno.h>


int main () {
	int a = syscall(380,NULL);
	int b = syscall(380,NULL,NULL);

	return 0;
}
