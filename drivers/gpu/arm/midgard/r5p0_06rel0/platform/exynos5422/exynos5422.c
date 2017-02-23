/*
 * Copyright (C) 2015 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <mali_kbase.h>

/* TODO: support DVFS */

static int exynos5422_platform_init(struct kbase_device *kbdev)
{
	return 0;
}

static void exynos5422_platform_term(struct kbase_device *kbdev)
{
}

static struct kbase_platform_funcs_conf exynos5422_platform_funcs = {
	.platform_init_func = exynos5422_platform_init,
	.platform_term_func = exynos5422_platform_term,
};

static const struct kbase_attribute exynos5422_attributes[] = {
	{
		KBASE_CONFIG_ATTR_PLATFORM_FUNCS,
		(uintptr_t)&exynos5422_platform_funcs,
	}, {
		KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS,
		500, /* 500ms before cancelling stuck jobs */
	}, {
		KBASE_CONFIG_ATTR_END,
		0,
	}
};

static struct kbase_platform_config exynos5422_platform_config = {
	.attributes = exynos5422_attributes,
};

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &exynos5422_platform_config;
}
