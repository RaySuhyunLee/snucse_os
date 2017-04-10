#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
//#include <linux/rotation.h>

int main (int argc, char *argv[]) {
	
	FILE *f;
	int for_lock;
	int for_unlock;
	char str[15];
	char * s = argv[1];
	int i =0;
	int arg = 0;
	int degree, range, option;

	while(s != NULL && *(s+i)!= NULL) arg = arg*10 + (*(s+(i++))-'0');
	
	while (1) {

		//use write_lock
		printf("Option(1:Lock/0:Unlock)  Degree, Range : ");
		scanf("%d %d %d",&option, &degree, &range);
		if(option == 0){
			for_unlock = syscall(385,degree,range);
			continue;
		}
		for_lock = syscall(382, degree, range);
		
		sprintf(str, "%d", arg);
		f = fopen("integer", "w");
		fputs(str, f);
		fclose(f);
		
		//use write_unlock
		printf("selector: %s \n", str);
		arg++;
	}
	return 0;
}
