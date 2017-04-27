#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>



int main () {
	int a = syscall(380,1,1);
	int b = syscall(381,1);



	return 0;
}
