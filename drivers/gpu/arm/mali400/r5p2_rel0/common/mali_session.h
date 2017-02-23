/*
 * Copyright (C) 2010-2015 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __MALI_SESSION_H__
#define __MALI_SESSION_H__

#include "mali_mmu_page_directory.h"
#include "mali_osk.h"
#include "mali_osk_list.h"
#include "mali_memory_types.h"
#include "mali_memory_manager.h"

struct mali_timeline_system;
struct mali_soft_system;

/* Number of frame builder job lists per session. */
#define MALI_PP_JOB_FB_LOOKUP_LIST_SIZE 16
#define MALI_PP_JOB_FB_LOOKUP_LIST_MASK (MALI_PP_JOB_FB_LOOKUP_LIST_SIZE - 1)
#if TIZEN_GLES_MEM_PROFILE
#define MALI_GLES_MEM_SHIFT	8

enum mali_session_gles_mem_profile_type {
	MALI_GLES_MEM_PP_READ			= (1 << 0),
	MALI_GLES_MEM_PP_WRITE			= (1 << 1),
	MALI_GLES_MEM_GP_READ			= (1 << 2),
	MALI_GLES_MEM_GP_WRITE			= (1 << 3),
	MALI_GLES_MEM_CPU_READ			= (1 << 4),
	MALI_GLES_MEM_CPU_WRITE			= (1 << 5),
	MALI_GLES_MEM_GP_L2_ALLOC		= (1 << 6),
	MALI_GLES_MEM_FRAME_POOL_ALLOC		= (1 << 7),
	/* type of this mem block */
	MALI_GLES_MEM_UNTYPED			= (0 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_VB_IB			= (1 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_TEXTURE			= (2 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_VARYING			= (3 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_RT			= (4 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_PBUFFER			= (5 << MALI_GLES_MEM_SHIFT),
	/* memory types for gp command */
	MALI_GLES_MEM_PLBU_HEAP			= (6 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_POINTER_ARRAY		= (7 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_SLAVE_TILE_LIST		= (8 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_UNTYPE_GP_CMD_LIST	= (9 << MALI_GLES_MEM_SHIFT),
	/* memory type for polygon list command */
	MALI_GLES_MEM_POLYGON_CMD_LIST		= (10 << MALI_GLES_MEM_SHIFT),
	/* memory types for pp command */
	MALI_GLES_MEM_TD			= (11 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_RSW			= (12 << MALI_GLES_MEM_SHIFT),
	/* other memory types */
	MALI_GLES_MEM_SHADER			= (13 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_STREAMS			= (14 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_FRAGMENT_STACK		= (15 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_UNIFORM			= (16 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_UNTYPE_FRAME_POOL		= (17 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_UNTYPE_SURFACE		= (18 << MALI_GLES_MEM_SHIFT),
	MALI_GLES_MEM_MAX			= (19 << MALI_GLES_MEM_SHIFT),
};

#define MALI_MAX_GLES_MEM_TYPES	(MALI_GLES_MEM_MAX >> MALI_GLES_MEM_SHIFT)

struct mali_session_gles_mem_profile_api_info {
	char api[64]; /**< API name */
	u16 id; /**< API ID */
	int size; /**< Total size for this API */
	_MALI_OSK_LIST_HEAD(link); /**< Link for the list of all EGL/GL API */
};

struct mali_session_gles_mem_profile_info {
	u32 type; /**< Memory type ID */
	s64 size; /**< Total sizes for this type */
	_mali_osk_mutex_t *lock; /**< Mutex lock */
	_mali_osk_list_t api_head; /**< List of all EGL/GL APIs for this type */
};

struct mali_session_gles_mem_profile_trash_info {
	u32 pid;
	char *comm;
	struct mali_session_gles_mem_profile_info info[MALI_MAX_GLES_MEM_TYPES];
};
#endif	/* TIZEN_GLES_MEM_PROFILE */

struct mali_session_data {
	_mali_osk_notification_queue_t *ioctl_queue;

	_mali_osk_mutex_t *memory_lock; /**< Lock protecting the vm manipulation */
#if 0
	_mali_osk_list_t memory_head; /**< Track all the memory allocated in this session, for freeing on abnormal termination */
#endif
	struct mali_page_directory *page_directory; /**< MMU page directory for this session */

	_MALI_OSK_LIST_HEAD(link); /**< Link for list of all sessions */
	_MALI_OSK_LIST_HEAD(pp_job_list); /**< List of all PP jobs on this session */

#if defined(CONFIG_MALI_DVFS)
	_mali_osk_atomic_t number_of_window_jobs; /**< Record the window jobs completed on this session in a period */
#endif

	_mali_osk_list_t pp_job_fb_lookup_list[MALI_PP_JOB_FB_LOOKUP_LIST_SIZE]; /**< List of PP job lists per frame builder id.  Used to link jobs from same frame builder. */

	struct mali_soft_job_system *soft_job_system; /**< Soft job system for this session. */
	struct mali_timeline_system *timeline_system; /**< Timeline system for this session. */

	mali_bool is_aborting; /**< MALI_TRUE if the session is aborting, MALI_FALSE if not. */
	mali_bool use_high_priority_job_queue; /**< If MALI_TRUE, jobs added from this session will use the high priority job queues. */
	u32 pid;
	char *comm;
	atomic_t mali_mem_array[MALI_MEM_TYPE_MAX]; /**< The array to record mem types' usage for this session. */
	atomic_t mali_mem_allocated_pages; /** The current allocated mali memory pages, which include mali os memory and mali dedicated memory.*/
	size_t max_mali_mem_allocated_size; /**< The past max mali memory allocated size, which include mali os memory and mali dedicated memory. */
	/* Added for new memroy system */
	struct mali_allocation_manager allocation_mgr;
#ifdef SPRD_GPU_BOOST
	int level;
#endif

#if TIZEN_GLES_MEM_PROFILE
	struct dentry *gles_mem_profile_dentry;
	struct mali_session_gles_mem_profile_info
				gles_mem_profile_info[MALI_MAX_GLES_MEM_TYPES];
#endif
};

_mali_osk_errcode_t mali_session_initialize(void);
void mali_session_terminate(void);

/* List of all sessions. Actual list head in mali_kernel_core.c */
extern _mali_osk_list_t mali_sessions;
/* Lock to protect modification and access to the mali_sessions list */
extern _mali_osk_spinlock_irq_t *mali_sessions_lock;

MALI_STATIC_INLINE void mali_session_lock(void)
{
	_mali_osk_spinlock_irq_lock(mali_sessions_lock);
}

MALI_STATIC_INLINE void mali_session_unlock(void)
{
	_mali_osk_spinlock_irq_unlock(mali_sessions_lock);
}

void mali_session_add(struct mali_session_data *session);
void mali_session_remove(struct mali_session_data *session);
u32 mali_session_get_count(void);

#define MALI_SESSION_FOREACH(session, tmp, link) \
	_MALI_OSK_LIST_FOREACHENTRY(session, tmp, &mali_sessions, struct mali_session_data, link)

MALI_STATIC_INLINE struct mali_page_directory *mali_session_get_page_directory(struct mali_session_data *session)
{
	return session->page_directory;
}

MALI_STATIC_INLINE void mali_session_memory_lock(struct mali_session_data *session)
{
	MALI_DEBUG_ASSERT_POINTER(session);
	_mali_osk_mutex_wait(session->memory_lock);
}

MALI_STATIC_INLINE void mali_session_memory_unlock(struct mali_session_data *session)
{
	MALI_DEBUG_ASSERT_POINTER(session);
	_mali_osk_mutex_signal(session->memory_lock);
}

MALI_STATIC_INLINE void mali_session_send_notification(struct mali_session_data *session, _mali_osk_notification_t *object)
{
	_mali_osk_notification_queue_send(session->ioctl_queue, object);
}

#if defined(CONFIG_MALI_DVFS)

MALI_STATIC_INLINE void mali_session_inc_num_window_jobs(struct mali_session_data *session)
{
	MALI_DEBUG_ASSERT_POINTER(session);
	_mali_osk_atomic_inc(&session->number_of_window_jobs);
}

/*
 * Get the max completed window jobs from all active session,
 * which will be used in  window render frame per sec calculate
 */
u32 mali_session_max_window_num(void);

#endif

void mali_session_memory_tracking(_mali_osk_print_ctx *print_ctx);

#if TIZEN_GLES_MEM_PROFILE
void mali_session_gles_mem_profile_api_add(
			struct mali_session_gles_mem_profile_info *info,
			struct mali_session_gles_mem_profile_api_info *api,
			_mali_osk_list_t *head);
s64 mali_session_gles_mem_profile_info_tracking(_mali_osk_print_ctx *print_ctx,
							void *data, bool trash);
#endif

#endif /* __MALI_SESSION_H__ */
