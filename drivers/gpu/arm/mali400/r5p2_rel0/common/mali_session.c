/*
 * Copyright (C) 2012-2015 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "mali_osk.h"
#include "mali_osk_list.h"
#include "mali_session.h"
#include "mali_ukk.h"
#if TIZEN_GLES_MEM_PROFILE
#include "mali_kernel_sysfs.h"
#endif

_MALI_OSK_LIST_HEAD(mali_sessions);
static u32 mali_session_count = 0;

_mali_osk_spinlock_irq_t *mali_sessions_lock = NULL;

_mali_osk_errcode_t mali_session_initialize(void)
{
	_MALI_OSK_INIT_LIST_HEAD(&mali_sessions);

	mali_sessions_lock = _mali_osk_spinlock_irq_init(
				     _MALI_OSK_LOCKFLAG_ORDERED,
				     _MALI_OSK_LOCK_ORDER_SESSIONS);
	if (NULL == mali_sessions_lock) {
		return _MALI_OSK_ERR_NOMEM;
	}

	return _MALI_OSK_ERR_OK;
}

void mali_session_terminate(void)
{
	if (NULL != mali_sessions_lock) {
		_mali_osk_spinlock_irq_term(mali_sessions_lock);
		mali_sessions_lock = NULL;
	}
}

#if TIZEN_GLES_MEM_PROFILE
static void mali_session_gles_mem_init(struct mali_session_data *session)
{
	struct mali_session_gles_mem_profile_info *info;
	int i;

	mali_sysfs_gles_mem_profile_add((void *)session);

	for (i = 0; i < MALI_MAX_GLES_MEM_TYPES; i++) {
		info = &(session->gles_mem_profile_info[i]);
		info->type = i;
		info->lock = _mali_osk_mutex_init(_MALI_OSK_LOCKFLAG_ORDERED,
						_MALI_OSK_LOCK_ORDER_SESSIONS);
		if (!info->lock)
			MALI_PRINT_ERROR(("Mutex init failure"));
		_MALI_OSK_INIT_LIST_HEAD(&(info->api_head));
	}
}

static void mali_session_gles_mem_profile_apis_remove(
				struct mali_session_gles_mem_profile_info *info)
{
	struct mali_session_gles_mem_profile_api_info *api, *tmp;

	_mali_osk_mutex_wait(info->lock);

	_MALI_OSK_LIST_FOREACHENTRY(api, tmp, &(info->api_head),
			struct mali_session_gles_mem_profile_api_info, link) {
		_mali_osk_list_delinit(&(api->link));
		_mali_osk_free(api);
	}

	_mali_osk_mutex_signal(info->lock);
}

static void mali_session_gles_mem_deinit(struct mali_session_data *session)
{
	struct mali_session_gles_mem_profile_trash_info *trash_info = NULL;
	struct mali_session_gles_mem_profile_info *info;
	int i;

	for (i = 0; i < MALI_MAX_GLES_MEM_TYPES; i++) {
		info = &(session->gles_mem_profile_info[i]);
		if (!info->lock)
			continue;

		if (info->size) {
			trash_info =
			(struct mali_session_gles_mem_profile_trash_info *)
				mali_sysfs_gles_mem_profile_move_to_trash(
							(void *)trash_info,
							(void *)session, i);
		} else
			mali_session_gles_mem_profile_apis_remove(info);

		_mali_osk_mutex_term(info->lock);
	}

	mali_sysfs_gles_mem_profile_remove((void *)session);
}
#endif	/* TIZEN_GLES_MEM_PROFILE */

void mali_session_add(struct mali_session_data *session)
{
	mali_session_lock();
	_mali_osk_list_add(&session->link, &mali_sessions);
	mali_session_count++;
	mali_session_unlock();

#if TIZEN_GLES_MEM_PROFILE
	mali_session_gles_mem_init(session);
#endif
}

void mali_session_remove(struct mali_session_data *session)
{
#if TIZEN_GLES_MEM_PROFILE
	mali_session_gles_mem_deinit(session);
#endif

	mali_session_lock();
	_mali_osk_list_delinit(&session->link);
	mali_session_count--;
	mali_session_unlock();
}

u32 mali_session_get_count(void)
{
	return mali_session_count;
}

/*
 * Get the max completed window jobs from all active session,
 * which will be used in window render frame per sec calculate
 */
#if defined(CONFIG_MALI_DVFS)
u32 mali_session_max_window_num(void)
{
	struct mali_session_data *session, *tmp;
	u32 max_window_num = 0;
	u32 tmp_number = 0;

	mali_session_lock();

	MALI_SESSION_FOREACH(session, tmp, link) {
		tmp_number = _mali_osk_atomic_xchg(
				     &session->number_of_window_jobs, 0);
		if (max_window_num < tmp_number) {
			max_window_num = tmp_number;
		}
	}

	mali_session_unlock();

	return max_window_num;
}
#endif

void mali_session_memory_tracking(_mali_osk_print_ctx *print_ctx)
{
	struct mali_session_data *session, *tmp;
	u32 mali_mem_usage;
	u32 total_mali_mem_size;

	MALI_DEBUG_ASSERT_POINTER(print_ctx);
	mali_session_lock();
	MALI_SESSION_FOREACH(session, tmp, link) {
		_mali_osk_ctxprintf(print_ctx, "  %-25s  %-10u  %-10u  %-15u  %-15u  %-10u  %-10u\n",
				    session->comm, session->pid,
				    (atomic_read(&session->mali_mem_allocated_pages)) * _MALI_OSK_MALI_PAGE_SIZE,
				    session->max_mali_mem_allocated_size,
				    (atomic_read(&session->mali_mem_array[MALI_MEM_EXTERNAL])) * _MALI_OSK_MALI_PAGE_SIZE,
				    (atomic_read(&session->mali_mem_array[MALI_MEM_UMP])) * _MALI_OSK_MALI_PAGE_SIZE,
				    (atomic_read(&session->mali_mem_array[MALI_MEM_DMA_BUF])) * _MALI_OSK_MALI_PAGE_SIZE
				   );
	}
	mali_session_unlock();
	mali_mem_usage  = _mali_ukk_report_memory_usage();
	total_mali_mem_size = _mali_ukk_report_total_memory_size();
	_mali_osk_ctxprintf(print_ctx, "Mali mem usage: %u\nMali mem limit: %u\n", mali_mem_usage, total_mali_mem_size);
}

#if TIZEN_GLES_MEM_PROFILE
void mali_session_gles_mem_profile_api_add(
			struct mali_session_gles_mem_profile_info *info,
			struct mali_session_gles_mem_profile_api_info *api,
			_mali_osk_list_t *head)
{
	if (!info->lock)
		return;

	_mali_osk_mutex_wait(info->lock);

	_mali_osk_list_add(&(api->link), head);

	_mali_osk_mutex_signal(info->lock);
}

static const char *mali_session_gles_mem_profile_to_str(
				enum mali_session_gles_mem_profile_type type)
{
	switch (type) {
	case MALI_GLES_MEM_UNTYPED:
		return "mali_gles_mem Untyped";
	case MALI_GLES_MEM_VB_IB:
		return "mali_gles_mem Vertex & Index buffer";
	case MALI_GLES_MEM_TEXTURE:
		return "mali_gles_mem Texture";
	case MALI_GLES_MEM_RSW:
		return "mali_gles_mem RSW";
	case MALI_GLES_MEM_PLBU_HEAP:
		return "mali_gles_mem PLBU heap";
	case MALI_GLES_MEM_UNTYPE_GP_CMD_LIST:
		return "mali_gles_mem Uuntyped GP cmd list";
	case MALI_GLES_MEM_SHADER:
		return "mali_gles_mem Shader";
	case MALI_GLES_MEM_TD:
		return "mali_gles_mem TD";
	case MALI_GLES_MEM_UNTYPE_FRAME_POOL:
		return "mali_gles_mem Untyped frame pool";
	case MALI_GLES_MEM_UNTYPE_SURFACE:
		return "mali_gles_mem Untyped surface";
	case MALI_GLES_MEM_RT:
		return "mali_gles_mem Render target";
	case MALI_GLES_MEM_VARYING:
		return "mali_gles_mem Varying buffer";
	case MALI_GLES_MEM_STREAMS:
		return "mali_gles_mem Streams buffer";
	case MALI_GLES_MEM_POINTER_ARRAY:
		return "mali_gles_mem Pointer array buffer";
	case MALI_GLES_MEM_SLAVE_TILE_LIST:
		return "mali_gles_mem Slave tile list buffer";
	case MALI_GLES_MEM_POLYGON_CMD_LIST:
		return "mali_gles_mem Polygon cmd list";
	case MALI_GLES_MEM_FRAGMENT_STACK:
		return "mali_gles_mem Fragment stack";
	case MALI_GLES_MEM_UNIFORM:
		return "mali_gles_mem Uniforms";
	case MALI_GLES_MEM_PBUFFER:
		return "mali_gles_mem Pbuffer";
	default:
		return "mali_gles_mem Untyped";
	}
}

static void mali_session_gles_mem_profile_api_tracking(
				_mali_osk_print_ctx *print_ctx,
				struct mali_session_gles_mem_profile_info *info)
{
	struct mali_session_gles_mem_profile_api_info *api, *tmp;

	_MALI_OSK_LIST_FOREACHENTRY(api, tmp, &(info->api_head),
			struct mali_session_gles_mem_profile_api_info, link) {
		_mali_osk_ctxprintf(print_ctx, "\t| %-36s\t%-30d\n",
					api->api, api->size);
	}
}

s64 mali_session_gles_mem_profile_info_tracking(_mali_osk_print_ctx *print_ctx,
							void *data, bool trash)
{
	struct mali_session_gles_mem_profile_trash_info *trash_info = NULL;
	struct mali_session_gles_mem_profile_info *info;
	struct mali_session_data *session = NULL;
	s64 total_size = 0;
	int i;

	MALI_DEBUG_ASSERT_POINTER(print_ctx);
	MALI_DEBUG_ASSERT_POINTER(data);

	if (trash) {
		trash_info =
			(struct mali_session_gles_mem_profile_trash_info *)data;
	} else
		session = (struct mali_session_data *)data;

	for (i = 0; i < MALI_MAX_GLES_MEM_TYPES; i++) {
		if (trash)
			info = &(trash_info->info[i]);
		else
			info = &(session->gles_mem_profile_info[i]);
		if (!info->lock)
			continue;

		_mali_osk_mutex_wait(info->lock);

		_mali_osk_ctxprintf(print_ctx, "%-36s\t%-30lld\n",
					mali_session_gles_mem_profile_to_str(
						i << MALI_GLES_MEM_SHIFT),
					info->size);
		mali_session_gles_mem_profile_api_tracking(print_ctx, info);
		total_size += info->size;

		_mali_osk_mutex_signal(info->lock);
	}

	return total_size;
}
#endif	/* TIZEN_GLES_MEM_PROFILE */
