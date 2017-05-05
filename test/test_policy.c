#include<stdio.h>
#include<stdlib.h>
#include<sched.h>
#include<unistd.h>

int main () {

	int a = 1;
	int s = syscall(156,getpid(),6,0);
	while(a++){
		if(a%100000 == 0 ) {
			printf("a\n");
			a = 1;
		}
	}
	return 0;
}
