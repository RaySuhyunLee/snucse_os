/*
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __TIZEN_GLOBAL_LOCK_H__
#define __TIZEN_GLOBAL_LOCK_H__

#include <linux/ioctl.h>

#define TGL_IOC_BASE	0x32
#define TGL_MAJOR	224

struct tgl_attribute {
	unsigned int key;
	unsigned int timeout_ms;
};

struct tgl_user_data {
	unsigned int key;
	unsigned int data1;
	unsigned int data2;
	unsigned int locked;
};

enum tgl_ioctls {
	TGL_INIT_LOCK = 1,
	TGL_DESTROY_LOCK,
	TGL_LOCK_LOCK,
	TGL_UNLOCK_LOCK,
	TGL_SET_DATA,
	TGL_GET_DATA,
	TGL_DUMP_LOCKS,
};

#define TGL_IOC_INIT_LOCK	_IOW(TGL_IOC_BASE, TGL_INIT_LOCK,	\
					struct tgl_attribute *)
#define TGL_IOC_DESTROY_LOCK	_IOW(TGL_IOC_BASE, TGL_DESTROY_LOCK,	\
					unsigned int)
#define TGL_IOC_LOCK_LOCK	_IOW(TGL_IOC_BASE, TGL_LOCK_LOCK,	\
					unsigned int)
#define TGL_IOC_UNLOCK_LOCK	_IOW(TGL_IOC_BASE, TGL_UNLOCK_LOCK,	\
					unsigned int)
#define TGL_IOC_SET_DATA	_IOW(TGL_IOC_BASE, TGL_SET_DATA,	\
					struct tgl_user_data *)
#define TGL_IOC_GET_DATA	_IOW(TGL_IOC_BASE, TGL_GET_DATA,	\
					struct tgl_user_data *)
#define TGL_IOC_DUMP_LOCKS	_IOW(TGL_IOC_BASE, TGL_DUMP_LOCKS, void *)

#endif /* __TIZEN_GLOBAL_LOCK_H__ */
