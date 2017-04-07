#define SYSCALL_SET_ROTATION 380

#include <signal.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include<stdio.h>
int main()
{
	int degree;
	while(1) {
		printf("Set_Rotation Degree : ");
		scanf("%d", &degree);
		syscall(SYSCALL_SET_ROTATION, degree);
	}
	return 0;
}
