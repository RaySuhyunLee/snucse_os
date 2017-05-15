#ifndef __LINUX_WRR_H
#define __LINUX_WRR_H
	
#define WRR_DEFAULT_WEIGHT 10
#define WRR_MAX_WEIGHT 20
#define WRR_MIN_WEIGHT 1	

extern int wrr_set_weight(struct sched_wrr_entity*, int);

#define for_each_sched_wrr_entity (wrr_se) \
 for (; wrr_se; wrr_se = wrr_se->parent)

#define wrr_get_weight(__wrr_entity, __var) \
	do { \
		read_lock(&(__wrr_entity)->weight_lock); \
		*(__var) = (__wrr_entity)->weight; \
		read_unlock(&(__wrr_entity)->weight_lock); \
	} while(0)

#endif
