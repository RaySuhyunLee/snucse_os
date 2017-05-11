#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>

int main (int argc, char* argv[]) {
 	int period = atoi(argv[1]);
	while(1) {
	 	system("cat /proc/sched_debug");
		usleep(period*1000); 
	}
	return 0;
}
