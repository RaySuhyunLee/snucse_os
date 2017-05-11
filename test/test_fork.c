#include <stdio.h>
#include <stdlib.h>

#define TOTALFORK 5 //총 생성해야할 프로세스 수

int main(int argc, char **argv) {
 	pid_t pids[TOTALFORK], pid;
	int runProcess = 0; //생성한 프로세스 수
	int state;
	int weight;
	if (argc < 2) {
		printf("Usage: ./test_fork.o [weight]\n");
		return 0;
	}
	if (syscall(380, getpid(), atoi(argv[1])) < 0) {
		printf("weight setting failed. try again.\n");
		return -1;
	}

	weight = syscall(381, getpid());
	printf("I am grandparent. My weight is %d\n", weight);
	while(runProcess < TOTALFORK) { //5개의 프로세스를 loop 를 이용하여 생성
		 //자식 프로세스 종료 대기 (각 프로세스가 따로 동작하고, 
		 //종료를 기다려야 할 경우에 사용
		sleep(3);
		pids[runProcess] = fork();//fork 생성
		//0보다 작을 경우 에러 (-1)
		if(pids[runProcess] < 0) {
			return -1;
		} else if(pids[runProcess] == 0) {//자식 프로세스
			int weight = syscall(381, getpid());
			for(int i=0; i<runProcess+1; i++)
				printf("  ");
			printf("ㄴHi! I am child %ld. My weight is %d\n", (long)getpid(), weight);
		} else { //부모 프로세스
			for(int i=0; i<runProcess; i++)
				printf("  ");
			printf("Fork! New child is %ld.\n", (long)pids[runProcess]);
			pid = wait(&state); 
			return 0;
		}
		runProcess++;
	}
	return 0;
}
