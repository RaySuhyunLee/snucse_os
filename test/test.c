#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
//#include "ptree.h"
#include <sys/types.h>

//need to also include the prinfo.h but where is it

struct prinfo {
	long state;
	pid_t pid;
	pid_t parent_pid;
	pid_t first_child_pid;
	pid_t next_sibling_pid;
	long uid;
	char comm[64];
};

void print_ptree_prinfo (/*parameters needed*/struct prinfo *ptree, int cnt_process) {
	
	int i = 0;
	int tab = 0;
	int level = 0;
	int p = 0;
	
	for(i = 0; i < cnt_process; i++) {		//should it start from i = 1 ?
		
		if( (ptree[i].next_sibling_pid == 0) && (i != 0) ) {
			level += 1;
		}
		
		for(p = 0; p < tab; p++) {
			printf("\t");
		}
		
		printf("%s,%d,%ld,%d,%d,%d,%d\n", ptree[i].comm, ptree[i].pid, ptree[i].state,
										ptree[i].parent_pid, ptree[i].first_child_pid,
										ptree[i].next_sibling_pid, ptree[i].uid);
		
		if(ptree[i].first_child_pid != 0) {
			tab += 1;
		}
		
		else if( (ptree[i].next_sibling_pid == 0) && (level > 0) ) {
			tab -= level;
			level = 0;
		}
	}
}

int main () {
	
	struct prinfo *buf;
	int nr=10;
	int process_cnt;
	
	buf = calloc(nr,sizeof(struct prinfo));	//calloc needed?
	
	printf("before syscall");

	process_cnt = syscall(380, buf, &nr);	//syscall num 380?
	
	printf("after syscall");

	if(process_cnt < 0) {
		perror("process count < 0");
		return -1;
	}
	
	print_ptree_prinfo(buf, nr);	//not process_cnt
	
	free(buf);
	
	return 0;
}
