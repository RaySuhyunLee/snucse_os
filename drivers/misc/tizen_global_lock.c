/*
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <asm/current.h>
#include <linux/sched.h>

#include "tizen_global_lock.h"

#define TGL_WARN(x, y...)	pr_warn("(%d,%d)(%s):%d " x "\n",	\
					current->tgid, current->pid,	\
					__func__, __LINE__, ##y)
#define TGL_INFO(x, y...)	pr_info(x, ##y)
#define TGL_LOG(x, y...)
#define TGL_DEBUG(x, y...)

static struct tgl_global {
	int major;
	struct class		*class;
	struct device		*device;
	void			*locks;		/* global lock table */
	int			refcnt;		/* ref count of tgl_global */
	struct mutex		mutex;
} tgl_global;

struct tgl_session_data {
	void		*inited_locks;	/* per session initialized locks */
	void		*locked_locks;	/* per session locked locks */
};

struct tgl_lock {
	unsigned int		key;		/* key of this lock */
	unsigned int		timeout_ms;	/* timeout in ms */
	unsigned int		refcnt;		/* ref count */
	wait_queue_head_t	waiting_queue;	/* waiting queue */
	struct list_head	waiting_list;	/* waiting list */
	struct mutex		waiting_list_mutex;
	unsigned int		locked;		/* flag if is locked */
	unsigned int		owner;		/* session data */

	struct mutex		data_mutex;
	unsigned int		user_data1;
	unsigned int		user_data2;

	pid_t			owner_pid;
	pid_t			owner_tid;
};

#define TGL_HASH_BITS		4
#define TGL_HASH_ENTRIES	(1 << TGL_HASH_BITS)

struct tgl_hash_head {
	struct hlist_head	head;		/* hash_head */
	struct mutex		mutex;
};

struct tgl_hash_node {
	unsigned int		key;		/* must be same as lock->key */
	struct tgl_lock		*lock;		/* lock object */
	struct hlist_node	node;		/* hash node */
};

static const char tgl_dev_name[] = "tgl";

/* find the tgl_lock object with key in the hash table */
static struct tgl_hash_node *tgl_hash_get_node(struct tgl_hash_head *hash,
					       unsigned int key)
{
	struct tgl_hash_head *hash_head = &hash[hash_32(key, TGL_HASH_BITS)];
	struct tgl_hash_node *hash_node = NULL;
	struct tgl_hash_node *found = NULL;

	struct hlist_head *head = &hash_head->head;

	TGL_LOG("key %d", key);

	mutex_lock(&hash_head->mutex);
	hlist_for_each_entry(hash_node, head, node) {
		if (hash_node->key == key) {
			found = hash_node;
			break;
		}
	}
	mutex_unlock(&hash_head->mutex);

	TGL_LOG("hash_node: %p", hash_node);

	return found;
}

/* insert the hash entry */
static struct tgl_hash_node *tgl_hash_insert_node(struct tgl_hash_head *hash,
						  unsigned int key)
{
	struct tgl_hash_head *hash_head = &hash[hash_32(key, TGL_HASH_BITS)];
	struct tgl_hash_node *hash_node;
	struct hlist_head *head = &hash_head->head;

	TGL_LOG("key %d", key);

	hash_node = kzalloc(sizeof(struct tgl_hash_node), GFP_KERNEL);
	if (hash_node == NULL)
		return NULL;

	INIT_HLIST_NODE(&hash_node->node);
	mutex_lock(&hash_head->mutex);
	hlist_add_head(&hash_node->node, head);
	mutex_unlock(&hash_head->mutex);

	hash_node->key = key;

	TGL_LOG();

	return hash_node;
}

/* remove the hash entry */
static int tgl_hash_remove_node(struct tgl_hash_head *hash, unsigned int key)
{
	struct tgl_hash_head *hash_head = &hash[hash_32(key, TGL_HASH_BITS)];
	struct tgl_hash_node *hash_node;
	struct hlist_head *head = &hash_head->head;
	int err = -ENOENT;

	TGL_LOG("key %d", key);

	mutex_lock(&hash_head->mutex);
	hlist_for_each_entry(hash_node, head, node) {
		if (hash_node->key == key) {
			hlist_del(&hash_node->node);
			kfree(hash_node);
			err = 0;
			break;
		}
	}
	mutex_unlock(&hash_head->mutex);

	TGL_LOG();

	return err;
}

static int tgl_hash_cleanup_nodes(struct tgl_hash_head *hash,
				  int (*lock_cleanup_func)(struct tgl_lock *))
{
	struct tgl_hash_node *hash_node;
	struct hlist_head *head;
	int i;
	int err = 0;

	TGL_LOG();

	for (i = 0; i < TGL_HASH_ENTRIES; i++) {
		head = &hash[i].head;
		mutex_lock(&hash->mutex);
		while (!hlist_empty(head)) {
			hash_node = hlist_entry(head->first,
					struct tgl_hash_node, node);
			if (lock_cleanup_func(hash_node->lock) < 0)
				err = -EBADRQC;
			hlist_del(&hash_node->node);
			kfree(hash_node);
		}
		mutex_unlock(&hash->mutex);
	}

	TGL_LOG();

	return err;
}

/* allocate the hash table */
static struct tgl_hash_head *tgl_hash_create_table(void)
{
	struct tgl_hash_head *hash;
	int i;

	TGL_LOG();

	hash = kzalloc(sizeof(*hash) * TGL_HASH_ENTRIES, GFP_KERNEL);
	if (hash == NULL)
		return NULL;

	for (i = 0; i < TGL_HASH_ENTRIES; i++) {
		INIT_HLIST_HEAD(&hash[i].head);
		mutex_init(&hash[i].mutex);
	}

	TGL_LOG();

	return hash;
}

/* release the hash table */
static void tgl_hash_destroy_table(struct tgl_hash_head *hash)
{
	TGL_LOG();

	kfree(hash);

	TGL_LOG();
}

static struct tgl_lock *tgl_get_lock(void *locks, unsigned int key)
{
	struct tgl_hash_node *hash_node;

	hash_node = tgl_hash_get_node((struct tgl_hash_head *)locks, key);
	if (hash_node == NULL)
		return NULL;

	return hash_node->lock;
}

static int tgl_insert_lock(void *locks, struct tgl_lock *lock)
{
	struct tgl_hash_node *hash_node;

	hash_node = tgl_hash_insert_node((struct tgl_hash_head *)locks,
			lock->key);
	if (hash_node == NULL)
		return -ENOMEM;
	hash_node->lock = lock;

	return 0;
}

static int tgl_remove_lock(void *locks, unsigned int key)
{
	return tgl_hash_remove_node((struct tgl_hash_head *)locks, key);
}

static int tgl_cleanup_locks(void *locks,
			     int (*lock_cleanup_func)(struct tgl_lock *))
{
	return tgl_hash_cleanup_nodes((struct tgl_hash_head *)locks,
			lock_cleanup_func);
}

static void *tgl_create_locks(void)
{
	return (void *)tgl_hash_create_table();
}

static void tgl_destroy_locks(void *locks)
{
	tgl_hash_destroy_table((struct tgl_hash_head *)locks);
}

static int tgl_lock_lock(struct tgl_session_data *session_data,
			 unsigned int key)
{
	struct tgl_lock *lock;
	struct list_head waiting_entry;
	unsigned long jiffies;
	long ret;

	TGL_LOG("key: %d", key);

	mutex_lock(&tgl_global.mutex);
	lock = tgl_get_lock(tgl_global.locks, key);
	if (lock == NULL) {
		if (tgl_get_lock(session_data->inited_locks, key))
			tgl_remove_lock(session_data->inited_locks, key);

		if (tgl_get_lock(session_data->locked_locks, key))
			tgl_remove_lock(session_data->locked_locks, key);
		mutex_unlock(&tgl_global.mutex);
		TGL_WARN("lock is not in the global locks");
		return -ENOENT;
	}

	lock = tgl_get_lock(session_data->inited_locks, key);
	if (lock == NULL) {
		mutex_unlock(&tgl_global.mutex);
		TGL_WARN("lock is not in the inited locks");
		return -ENOENT;
	}
	mutex_unlock(&tgl_global.mutex);

	INIT_LIST_HEAD(&waiting_entry);
	mutex_lock(&lock->data_mutex);
	lock->refcnt++;
	mutex_unlock(&lock->data_mutex);
	mutex_lock(&lock->waiting_list_mutex);
	list_add_tail(&waiting_entry, &lock->waiting_list);
	mutex_unlock(&lock->waiting_list_mutex);

	jiffies = msecs_to_jiffies(lock->timeout_ms);

	TGL_LOG();

	ret = wait_event_timeout(lock->waiting_queue,
			((lock->locked == 0)
			 && lock->waiting_list.next == &waiting_entry),
			jiffies);
	if (ret == 0) {
		TGL_WARN("timed out, key: %d, owner(%d, %d)",
				key, lock->owner_pid, lock->owner_tid);
		mutex_lock(&lock->data_mutex);
		lock->refcnt--;
		mutex_unlock(&lock->data_mutex);
		mutex_lock(&lock->waiting_list_mutex);
		list_del(&waiting_entry);
		mutex_unlock(&lock->waiting_list_mutex);
		return -ETIMEDOUT;
	}

	TGL_LOG();

	mutex_lock(&lock->data_mutex);
	lock->owner = (unsigned int)session_data;
	lock->locked = 1;
	lock->owner_pid = current->tgid;
	lock->owner_tid = current->pid;
	mutex_unlock(&lock->data_mutex);

	mutex_lock(&lock->waiting_list_mutex);
	list_del(&waiting_entry);
	mutex_unlock(&lock->waiting_list_mutex);

	/* add to the locked lock */
	tgl_insert_lock(session_data->locked_locks, lock);

	TGL_LOG();

	return 0;
}

static int _tgl_unlock_lock(struct tgl_lock *lock)
{
	TGL_LOG();

	if (lock == NULL) {
		TGL_WARN("lock == NULL");
		return -EBADRQC;
	}
	mutex_lock(&lock->data_mutex);

	if (lock->locked == 0) {
		mutex_unlock(&lock->data_mutex);
		TGL_WARN("tried to unlock not-locked lock");
		return -EBADRQC;
	}

	lock->owner = 0;
	lock->locked = 0;
	lock->owner_pid = 0;
	lock->owner_tid = 0;
	lock->refcnt--;

	if (waitqueue_active(&lock->waiting_queue))
		wake_up(&lock->waiting_queue);

	mutex_unlock(&lock->data_mutex);

	TGL_LOG();

	return 0;
}

static int tgl_unlock_lock(struct tgl_session_data *session_data,
			   unsigned int key)
{
	struct tgl_lock *lock;
	int err;

	TGL_LOG("key: %d", key);

	mutex_lock(&tgl_global.mutex);
	lock = tgl_get_lock(tgl_global.locks, key);
	if (lock == NULL) {
		if (tgl_get_lock(session_data->inited_locks, key))
			tgl_remove_lock(session_data->inited_locks, key);

		if (tgl_get_lock(session_data->locked_locks, key))
			tgl_remove_lock(session_data->locked_locks, key);
		mutex_unlock(&tgl_global.mutex);
		TGL_WARN("lock is not in the global locks");
		return -ENOENT;
	}

	lock = tgl_get_lock(session_data->inited_locks, key);
	if (lock == NULL) {
		mutex_unlock(&tgl_global.mutex);
		TGL_WARN("lock is not in the inited locks");
		return -ENOENT;
	}
	mutex_unlock(&tgl_global.mutex);

	mutex_lock(&lock->data_mutex);
	if (lock->owner != (unsigned int)session_data) {
		mutex_unlock(&lock->data_mutex);
		TGL_WARN("tried to unlock not-owned lock by calling session");
		return -EBADRQC;
	}
	mutex_unlock(&lock->data_mutex);
	tgl_remove_lock(session_data->locked_locks, key);
	err = _tgl_unlock_lock(lock);
	if (err < 0)
		TGL_WARN("_tgl_unlock_lock() failed");

	if (err < 0)
		TGL_WARN("tgl_remove_lock() failed");

	TGL_LOG();

	return err;
}

static int tgl_init_lock(struct tgl_session_data *session_data,
			 struct tgl_attribute *attr)
{
	struct tgl_lock *lock;
	int err;

	TGL_LOG("key: %d", attr->key);

	mutex_lock(&tgl_global.mutex);

	lock = tgl_get_lock(tgl_global.locks, attr->key);
	if (lock == NULL) {
		/*
		 * allocate and add to the global table if this is the first
		 * initialization
		 */
		lock = kzalloc(sizeof(struct tgl_lock), GFP_KERNEL);
		if (lock == NULL) {
			err = -ENOMEM;
			goto out_unlock;
		}

		lock->key = attr->key;

		err = tgl_insert_lock(tgl_global.locks, lock);
		if (err < 0)
			goto out_unlock;

		/* default timeout value is 16ms */
		lock->timeout_ms = attr->timeout_ms ? attr->timeout_ms : 16;

		init_waitqueue_head(&lock->waiting_queue);
		INIT_LIST_HEAD(&lock->waiting_list);
		mutex_init(&lock->waiting_list_mutex);
		mutex_init(&lock->data_mutex);
	}
	mutex_lock(&lock->data_mutex);
	lock->refcnt++;
	mutex_unlock(&lock->data_mutex);

	/* add to the inited locks */
	err = tgl_insert_lock(session_data->inited_locks, lock);

out_unlock:

	mutex_unlock(&tgl_global.mutex);

	TGL_LOG();

	return err;
}

static int _tgl_destroy_lock(struct tgl_lock *lock)
{
	int err;

	TGL_LOG();

	if (lock == NULL) {
		TGL_WARN("lock == NULL");
		return -EBADRQC;
	}

	mutex_lock(&lock->data_mutex);
	lock->refcnt--;
	if (lock->refcnt == 0) {
		mutex_unlock(&lock->data_mutex);
		err = tgl_remove_lock(tgl_global.locks, lock->key);
		if (err < 0)
			return err;

		kfree(lock);
	} else
		mutex_unlock(&lock->data_mutex);

	TGL_LOG();

	return 0;
}

static int tgl_destroy_lock(struct tgl_session_data *session_data,
			    unsigned int key)
{
	struct tgl_lock *lock;

	int err;

	TGL_LOG();

	mutex_lock(&tgl_global.mutex);

	lock = tgl_get_lock(tgl_global.locks, key);
	if (lock == NULL) {
		TGL_WARN("lock is not in the global locks");
		err = -ENOENT;
		goto out_unlock;
	}
	if (!tgl_get_lock(session_data->inited_locks, key)) {
		TGL_WARN("lock is not in the inited locks");
		err = -ENOENT;
		goto out_unlock;
	}

	/* check if lock is still locked */
	if (tgl_get_lock(session_data->locked_locks, key)) {
		TGL_WARN("destroy failed. lock is still locked");
		err = -EBUSY;
		goto out_unlock;
	}

	err = _tgl_destroy_lock(lock);
	if (err < 0)
		goto out_unlock;

	/* remove from the inited lock */
	err = tgl_remove_lock(session_data->inited_locks, key);
	if (err < 0)
		goto out_unlock;

out_unlock:
	mutex_unlock(&tgl_global.mutex);

	TGL_LOG();

	return err;
}

static int tgl_set_data(struct tgl_session_data *session_data,
			struct tgl_user_data *user_data)
{
	struct tgl_lock *lock;
	unsigned int key = user_data->key;

	TGL_LOG("key: %d", key);

	mutex_lock(&tgl_global.mutex);

	lock = tgl_get_lock(tgl_global.locks, key);
	if (lock == NULL) {
		TGL_WARN("lock is not in the inited locks");
		mutex_unlock(&tgl_global.mutex);
		return -ENOENT;
	}
	mutex_lock(&lock->data_mutex);
	lock->user_data1 = user_data->data1;
	lock->user_data2 = user_data->data2;
	user_data->locked = lock->locked;
	mutex_unlock(&lock->data_mutex);
	mutex_unlock(&tgl_global.mutex);

	TGL_LOG();

	return 0;
}

static int tgl_get_data(struct tgl_session_data *session_data,
			struct tgl_user_data *user_data)
{
	struct tgl_lock *lock;
	unsigned int key = user_data->key;

	TGL_LOG("key: %d", key);
	mutex_lock(&tgl_global.mutex);

	lock = tgl_get_lock(tgl_global.locks, key);
	if (lock == NULL) {
		TGL_WARN("lock is not in the inited locks");
		mutex_unlock(&tgl_global.mutex);
		return -ENOENT;
	}
	mutex_lock(&lock->data_mutex);
	user_data->data1 = lock->user_data1;
	user_data->data2 = lock->user_data2;
	user_data->locked = lock->locked;
	mutex_unlock(&lock->data_mutex);
	mutex_unlock(&tgl_global.mutex);

	TGL_LOG();

	return 0;
}

static void tgl_dump_locks(void)
{
	int i;

	TGL_INFO("TIZEN_GLOBAL_LOCK DUMP START\n");

	mutex_lock(&tgl_global.mutex);
	for (i = 0; i < TGL_HASH_ENTRIES; i++) {
		struct tgl_hash_head *shead;
		struct tgl_hash_node *snode;
		struct hlist_head *hhead;

		shead = &((struct tgl_hash_head *)tgl_global.locks)[i];
		if (!shead)
			continue;
		mutex_lock(&shead->mutex);
		hhead = &shead->head;
		hlist_for_each_entry(snode, hhead, node) {
			struct tgl_lock *lock = snode->lock;

			mutex_lock(&lock->data_mutex);
			TGL_INFO("lock key: %d, refcnt: %d, pid: %d, tid: %d\n",
					lock->key, lock->refcnt,
					lock->owner_pid, lock->owner_tid);
			mutex_unlock(&lock->data_mutex);
		}
		mutex_unlock(&shead->mutex);
	}
	mutex_unlock(&tgl_global.mutex);

	TGL_INFO("TIZEN_GLOBAL_LOCK DUMP END\n");
}

static long tgl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct tgl_session_data *session_data = file->private_data;
	int err = 0;

	TGL_LOG();

	switch (cmd) {
	case TGL_IOC_INIT_LOCK:
		/* destroy lock with attribute */
		err = tgl_init_lock(session_data, (struct tgl_attribute *)arg);
		break;
	case TGL_IOC_DESTROY_LOCK:
		/* destroy lock with id(=arg) */
		err = tgl_destroy_lock(session_data, (unsigned int)arg);
		break;
	case TGL_IOC_LOCK_LOCK:
		/* lock lock with id(=arg) */
		err = tgl_lock_lock(session_data, (unsigned int)arg);
		break;
	case TGL_IOC_UNLOCK_LOCK:
		/* unlock lock with id(=arg) */
		err = tgl_unlock_lock(session_data, (unsigned int)arg);
		break;
	case TGL_IOC_SET_DATA:
		err = tgl_set_data(session_data, (struct tgl_user_data *)arg);
		break;
	case TGL_IOC_GET_DATA:
		err = tgl_get_data(session_data, (struct tgl_user_data *)arg);
		break;
	case TGL_IOC_DUMP_LOCKS:
		tgl_dump_locks();
		break;
	default:
		TGL_WARN("unknown type of ioctl command");
		break;
	}

	TGL_LOG();

	return err;
}

static int tgl_open(struct inode *inode, struct file *file)
{
	struct tgl_session_data *session_data;

	TGL_LOG();

	/* init per thread data using file->private_data*/
	session_data = kzalloc(sizeof(struct tgl_session_data), GFP_KERNEL);
	if (session_data == NULL)
		goto err_session_data;

	session_data->inited_locks = tgl_create_locks();
	if (session_data->inited_locks == NULL)
		goto err_inited_locks;

	session_data->locked_locks = tgl_create_locks();
	if (session_data->locked_locks == NULL)
		goto err_locked_locks;

	file->private_data = (void *)session_data;

	tgl_global.refcnt++;

	TGL_LOG();

	return 0;

err_locked_locks:
	tgl_destroy_locks(session_data->inited_locks);
err_inited_locks:
	kfree(session_data);
err_session_data:
	TGL_WARN();
	return -ENOMEM;
}

static int tgl_release(struct inode *inode, struct file *file)
{
	struct tgl_session_data *session_data = file->private_data;

	TGL_LOG();

	mutex_lock(&tgl_global.mutex);

	/* clean up the locked locks */
	if (tgl_cleanup_locks(session_data->locked_locks, _tgl_unlock_lock))
		TGL_WARN("clean-up locked locks failed");

	/* clean up the inited locks */
	if (tgl_cleanup_locks(session_data->inited_locks, _tgl_destroy_lock))
		TGL_WARN("clean-up inited locks failed");

	/* clean up per thread data */
	file->private_data = NULL;

	tgl_destroy_locks(session_data->locked_locks);
	tgl_destroy_locks(session_data->inited_locks);

	kfree(session_data);

	mutex_unlock(&tgl_global.mutex);
	tgl_global.refcnt--;
	if (tgl_global.refcnt == 0) {
		/* destroy global lock table */
		tgl_destroy_locks(tgl_global.locks);

		device_destroy(tgl_global.class, MKDEV(tgl_global.major, 0));
		class_destroy(tgl_global.class);
		unregister_chrdev(tgl_global.major, tgl_dev_name);
	}

	TGL_LOG();

	return 0;
}

static const struct file_operations tgl_ops = {
	.owner = THIS_MODULE,
	.open = tgl_open,
	.release = tgl_release,
	.unlocked_ioctl = tgl_ioctl,
};

static int __init tgl_init(void)
{
	int err;

	TGL_LOG();

	memset(&tgl_global, 0, sizeof(struct tgl_global));

	tgl_global.major = TGL_MAJOR;
	err = register_chrdev(tgl_global.major, tgl_dev_name, &tgl_ops);
	if (err < 0)
		goto err_register_chrdev;

	tgl_global.class = class_create(THIS_MODULE, tgl_dev_name);
	if (IS_ERR(tgl_global.class)) {
		err = PTR_ERR(tgl_global.class);
		goto err_class_create;
	}

	tgl_global.device = device_create(tgl_global.class, NULL,
			MKDEV(tgl_global.major, 0), NULL, tgl_dev_name);
	if (IS_ERR(tgl_global.device)) {
		err = PTR_ERR(tgl_global.device);
		goto err_device_create;
	}

	/* create the global lock table */
	tgl_global.locks = tgl_create_locks();
	if (tgl_global.locks == NULL) {
		err = -ENOMEM;
		goto err_create_locks;
	}

	mutex_init(&tgl_global.mutex);

	tgl_global.refcnt++;

	TGL_LOG();

	return 0;

err_create_locks:
err_device_create:
	class_unregister(tgl_global.class);
err_class_create:
	unregister_chrdev(tgl_global.major, tgl_dev_name);
err_register_chrdev:
	TGL_WARN();
	return err;
}

void tgl_exit(void)
{
	TGL_LOG();

	tgl_global.refcnt--;
	if (tgl_global.refcnt == 0) {
		mutex_destroy(&tgl_global.mutex);

		/* destroy global lock table */
		tgl_destroy_locks(tgl_global.locks);

		device_destroy(tgl_global.class, MKDEV(tgl_global.major, 0));
		class_destroy(tgl_global.class);
		unregister_chrdev(tgl_global.major, tgl_dev_name);
	}

	TGL_LOG();
}

module_init(tgl_init);
module_exit(tgl_exit);

MODULE_LICENSE("GPL");
