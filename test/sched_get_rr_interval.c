#include <stdio.h>
#include <sched.h>

int main()
{
	struct timespec ts;
	int ret;
	int pid;
	scanf("%d",&pid);
	/* real apps must check return values */
	ret = sched_rr_get_interval(pid, &ts);

	printf("Timeslice: %lu.%lu\n", ts.tv_sec, ts.tv_nsec);
	return 0;
}
