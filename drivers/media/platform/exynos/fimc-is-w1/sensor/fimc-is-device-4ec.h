/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_DEVICE_4EC_H
#define FIMC_IS_DEVICE_4EC_H

/* #define CONFIG_LOAD_FILE */

#if defined(CONFIG_LOAD_FILE)
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define LOAD_FILE_PATH "/root/fimc-is-device-4ec-reg.h"
#endif

#define SENSOR_S5K4EC_INSTANCE	0
#define SENSOR_S5K4EC_NAME	SENSOR_NAME_S5K4EC
#define SENSOR_S5K4EC_DRIVING

#define TAG_NAME	"[4EC] "
#define cam_err(fmt, ...)	\
	pr_err(TAG_NAME "[ERR]%s:%d: " fmt, __func__, __LINE__, \
	##__VA_ARGS__)
#define cam_warn(fmt, ...)	\
	pr_warn(TAG_NAME "%s: " fmt, __func__, \
	##__VA_ARGS__)
#define cam_info(fmt, ...)	\
	pr_info(TAG_NAME "%s: " fmt, __func__, \
	##__VA_ARGS__)
#if defined(CONFIG_CAM_DEBUG)
#define cam_dbg(fmt, ...)	\
	pr_debug(TAG_NAME fmt, ##__VA_ARGS__)
#else
#define cam_dbg(fmt, ...)
#endif

enum cam_scene_mode {
	SCENE_MODE_BASE,
	SCENE_MODE_NONE,
	SCENE_MODE_PORTRAIT,
	SCENE_MODE_NIGHTSHOT,
	SCENE_MODE_BACK_LIGHT,
	SCENE_MODE_LANDSCAPE,
	SCENE_MODE_SPORTS,
	SCENE_MODE_PARTY_INDOOR,
	SCENE_MODE_BEACH_SNOW,
	SCENE_MODE_SUNSET,
	SCENE_MODE_DUSK_DAWN,
	SCENE_MODE_FALL_COLOR,
	SCENE_MODE_FIREWORKS,
	SCENE_MODE_TEXT,
	SCENE_MODE_CANDLE_LIGHT,
	SCENE_MODE_LOW_LIGHT,
	SCENE_MODE_MAX,
};

#define V4L2_CID_CAMERA_AE_LOCK_UNLOCK	(V4L2_CID_PRIVATE_BASE + 144)
enum v4l2_ae_lockunlock {
	AE_UNLOCK = 0,
	AE_LOCK,
	AE_LOCK_MAX
};

#define V4L2_CID_CAMERA_AWB_LOCK_UNLOCK	(V4L2_CID_PRIVATE_BASE + 145)
enum v4l2_awb_lockunlock {
	AWB_UNLOCK = 0,
	AWB_LOCK,
	AWB_LOCK_MAX
};

#define V4L2_FOCUS_MODE_DEFAULT (1 << 8)
#define V4L2_CID_CAMERA_POWER_OFF	(V4L2_CID_PRIVATE_BASE + 330)
#define V4L2_CID_CAM_SINGLE_AUTO_FOCUS	(V4L2_CID_CAMERA_CLASS_BASE + 63)
#define V4L2_CID_EXIF_SHUTTER_SPEED_NUM (V4L2_CID_CAMERA_CLASS_BASE + 78)
#define V4L2_CID_EXIF_SHUTTER_SPEED_DEN (V4L2_CID_CAMERA_CLASS_BASE + 79)
#define V4L2_CID_EXIF_EXPOSURE_TIME_NUM (V4L2_CID_CAMERA_CLASS_BASE + 80)
#define V4L2_CID_EXIF_EXPOSURE_TIME_DEN (V4L2_CID_CAMERA_CLASS_BASE + 81)
#define V4L2_CID_CAMERA_AUTO_FOCUS_DONE	(V4L2_CID_CAMERA_CLASS_BASE+69)

enum {
	AUTO_FOCUS_FAILED,
	AUTO_FOCUS_DONE,
	AUTO_FOCUS_CANCELLED,
};

enum af_operation_status {
	AF_NONE = 0,
	AF_START,
	AF_CANCEL,
};

enum capture_flash_status {
	FLASH_STATUS_OFF = 0,
	FLASH_STATUS_PRE_ON,
	FLASH_STATUS_MAIN_ON,
};

/* result values returned to HAL */
enum af_result_status {
	AF_RESULT_NONE = 0x00,
	AF_RESULT_FAILED = 0x01,
	AF_RESULT_SUCCESS = 0x02,
	AF_RESULT_CANCELLED = 0x04,
	AF_RESULT_DOING = 0x08
};

struct sensor4ec_exif {
	u32 exp_time_num;
	u32 exp_time_den;
	u16 iso;
	u16 flash;
};

/* EXIF - flash filed */
#define EXIF_FLASH_OFF		(0x00)
#define EXIF_FLASH_FIRED		(0x01)
#define EXIF_FLASH_MODE_FIRING		(0x01 << 3)
#define EXIF_FLASH_MODE_SUPPRESSION	(0x02 << 3)
#define EXIF_FLASH_MODE_AUTO		(0x03 << 3)

/* Sensor AF first,second window size.
 * we use constant values instead of reading sensor register */
#define FIRST_WINSIZE_X			512
#define FIRST_WINSIZE_Y			568
#define SCND_WINSIZE_X			116	/* 230 -> 116 */
#define SCND_WINSIZE_Y			306

struct s5k4ec_rect {
	s32 x;
	s32 y;
	u32 width;
	u32 height;
};

struct s5k4ec_focus {
	u32 pos_x;
	u32 pos_y;
	u32 touch:1;
	u32 reset_done:1;
	u32 window_verified:1;	/* touch window verified */
};

struct s5k4ec_framesize {
	u32 index;
	u32 width;
	u32 height;
};

struct s5k4ec_state {
	struct v4l2_subdev	*subdev;
	struct fimc_is_image	image;
	struct mutex		ctrl_lock;
	struct mutex		af_lock;
	struct mutex		i2c_lock;
	struct completion	af_complete;

	u16			sensor_version;
	u32			setfile_index;
	u32			mode;
	u32			contrast;
	u32			effect;
	u32			ev;
	u32			flash_mode;
	u32			focus_mode;
	u32			iso;
	u32			metering;
	u32			saturation;
	u32			scene_mode;
	u32			sharpness;
	u32			white_balance;
	u32			anti_banding;
	u32			fps;
	bool			ae_lock;
	bool			awb_lock;
	bool			user_ae_lock;
	bool			user_awb_lock;
	bool			sensor_af_in_low_light_mode;
	bool			flash_fire;
	u32			sensor_mode;
	u32			light_level;

	enum af_operation_status	af_status;
	enum capture_flash_status flash_status;
	u32 af_result;
	struct work_struct set_focus_mode_work;
	struct work_struct af_work;
	struct work_struct af_stop_work;
	struct sensor4ec_exif	exif;
	struct s5k4ec_focus focus;
	struct s5k4ec_framesize preview;
};

static int sensor_4ec_pre_flash_start(struct v4l2_subdev *subdev);
static int sensor_4ec_pre_flash_stop(struct v4l2_subdev *subdev);
static int sensor_4ec_main_flash_start(struct v4l2_subdev *subdev);
static int sensor_4ec_main_flash_stop(struct v4l2_subdev *subdev);
static int sensor_4ec_auto_focus_proc(struct v4l2_subdev *subdev);
static int sensor_4ec_get_exif(struct v4l2_subdev *subdev);
int sensor_4ec_probe(struct i2c_client *client,
	const struct i2c_device_id *id);

#endif
