#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
//#include <linux/rotation.h>


static int run_flag= 1;

void interruptHandler (int a){
	run_flag = 0;
}

int main (int argc, char* argv[]){
	
	FILE *f;
	int for_lock;
	int for_unlock;
	char *str = argv[1];
	int arg=0;
	int tmp;
	int n;
	int i=0;
	int scan_ret;
	int flag ;
//	while(*(str+i) != NULL) arg = arg*10 + (*(str+ (i++))-'0');
	signal(SIGINT, interruptHandler);
	while (run_flag) {
	
		//use read_lock
		for_lock = syscall(381, 120, 30);
		
		f = fopen("integer", "r");
		scan_ret = fscanf(f, "%d", &tmp);
		printf("trial_%s: %d = ", str, tmp);
		flag= 0;
		n = tmp;
		if(n == 1) printf("%d\n",n);
		else {
			for(i = 2 ; i <= tmp; i++) { // it need to convert Trial and Division Method
				if(n%i == 0) {
					if(flag) printf(" * ");
					n/= i;
					printf("%d",i--);
					flag = 1;
				}
			}
		}
		printf("\n");
		fclose(f);
		//use read_unlock
		for_unlock = syscall(383, 120, 30);
		sleep(1);
	}
	printf("THEEND\n\n\n\n\n\n");
	return 0;
}
