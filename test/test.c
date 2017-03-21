#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#define NR_MAX 100	//the number of process to be printed

struct prinfo {
    long state;              //current state of process
    pid_t pid;               //process id 
    pid_t parent_pid;        //process id of parent 
    pid_t first_child_pid;   //pid of oldest child 
    pid_t next_sibling_pid;  //pid of younger sibling 
    long uid;                //user id of process owner 
    char comm[64];           //name of program executed 
};

void print_ptree_prinfo (struct prinfo *ptree, int cnt_process) {
	
	int i = 0;
	int tab = 0;
	int level = 0;
	int p = 0;
	int tmp[NR_MAX];
	int index = 0;
	
	for(i = 0; i < cnt_process; i++) {
	
		if(ptree[i].next_sibling_pid != 0) {	//has a sibling, make a branch point
			tmp[index] = tab;
			index++;
		}
		
		for(p = 0; p < tab; p++) {	//print the current process
			printf("\t");
		}
		
		printf("%s,%d,%ld,%d,%d,%d,%d\n", ptree[i].comm, ptree[i].pid, ptree[i].state,
										ptree[i].parent_pid, ptree[i].first_child_pid,
										ptree[i].next_sibling_pid, ptree[i].uid);
		
		if(ptree[i].first_child_pid != 0) {
			tab++;
		}
		else {	//no child, back to the most recent branch
			index = index - 1;
			tab = tmp[index];
		}
	}
}

int main () {
	
	struct prinfo *buf;
	int nr = NR_MAX;
	int process_cnt;
	
	buf = calloc(nr,sizeof(struct prinfo));	
	
	process_cnt = syscall(380, buf, &nr);
	
	/*
	if(process_cnt < 0) {
		perror("process count < 0");
		return -1;
	}
	*/
	
	print_ptree_prinfo(buf, nr);	
	
	free(buf);
	
	return 0;
}
