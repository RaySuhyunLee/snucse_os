#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

// -lpthread need to link

#define UPPER  10000 * 5
pthread_t threads[200];
int done[200];

void *thread_main(void *);

int main(int argc, char* argv[]){
	int i;
	int rc;
	int status;
//	int LIMIT = atoi(argv[1]);
	int LIMIT = 20;
 	struct timespec before, after ;
 	clock_gettime(CLOCK_MONOTONIC, &before);
	FILE *out_file;
	long result;
	char buf[40];
	
	printf("pid=%ld\n", (long)getpid());
	for (i = 0; i < LIMIT; i++)
	{	
	 done[i] = 0;
	 pthread_create(&threads[i], NULL, &thread_main, (void *)i);
	}
	for (i = LIMIT-1; i >= 0; i--)
	{
	 done[i] = 1;
	 rc = pthread_join(threads[i], (void **)&status);
	 if (rc == 0)
	 {
	  printf("Completed join with thread %d status= %d\n",i, status);
	 }
	 else
	 {
	  printf("ERROR; return code from pthread_join() is %d, thread %d\n", rc, i);
	  return -1;
	 }
	}
	clock_gettime(CLOCK_MONOTONIC, &after);
	result = after.tv_sec - before.tv_sec;
	
	sprintf(buf, "./result/pid_%d.result", getpid());
	out_file = fopen(buf, "w");
	fprintf(out_file, "Running Time : %ld s\n", result);
	fclose(out_file);
	return 0;
}
void *thread_main(void *arg)
{
	long long tmp, i;
	long long upper_bound = UPPER;    // = atoi(argv[2]);
	long long c = 1;
	pid_t pid = getpid();
	while (c <= upper_bound) {
		printf("trial %ld : %lld = ",(long)pid, c); 
		tmp = c;

		for(i=2;i<=tmp;i++) {
			if(tmp % i == 0) {
				printf("%lld",i);
				tmp = tmp / i;
				if(tmp % i == 0) printf("* ");
				else if(tmp % i != 0) {
					if(tmp > i)
					printf(" * ");
				}
				i = 1;
			}
		}
		c++;
		printf("\n");
	}
	
 	pthread_exit((void *) 0);
}

