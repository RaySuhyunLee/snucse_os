#include <stdio.h>
#include <stdlib.h>

#define TOTALFORK 5 //총 생성해야할 프로세스 수

int main(int argc, char **argv) {
 	pid_t pids[TOTALFORK], pid;
	int runProcess = 0; //생성한 프로세스 수
	int state;
	while(runProcess < TOTALFORK) { //5개의 프로세스를 loop 를 이용하여 생성
	 //자식 프로세스 종료 대기 (각 프로세스가 따로 동작하고, 
	 //종료를 기다려야 할 경우에 사용
		pid = wait(&state); 
		pids[runProcess] = fork();//fork 생성
		//0보다 작을 경우 에러 (-1)
		if(pids[runProcess] < 0) {
			return -1;
		} else if(pids[runProcess] == 0) {//자식 프로세스
			printf("child  %ld\n", (long)getpid());
		//for(;;) {}
			sleep(10);
			exit(0);
		} else { //부모 프로세스
			printf("parent %ld, child %ld\n", (long)getpid(), (long)pids[runProcess]);
		}
		runProcess++;
		}
		return 0;
}		
