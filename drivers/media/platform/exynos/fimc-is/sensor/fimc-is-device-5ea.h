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

#ifndef FIMC_IS_DEVICE_5EA_H
#define FIMC_IS_DEVICE_5EA_H

#define SENSOR_S5K5EA_INSTANCE	1
#define SENSOR_S5K5EA_NAME		SENSOR_NAME_S5K5EA
#define SENSOR_S5K5EA_DRIVING

#define TAG_NAME	"[5EA] "
#define cam_err(fmt, ...)	\
	pr_err(TAG_NAME fmt, ##__VA_ARGS__)
#define cam_warn(fmt, ...)	\
	pr_warn(TAG_NAME fmt, ##__VA_ARGS__)
#define cam_info(fmt, ...)	\
	pr_info(TAG_NAME fmt, ##__VA_ARGS__)
#if defined(CONFIG_CAM_DEBUG)
#define cam_dbg(fmt, ...)	\
	pr_debug(TAG_NAME fmt, ##__VA_ARGS__)
#else
#define cam_dbg(fmt, ...)
#endif

#define SENSOR_NAME "S5K5EA"

#define DEFAULT_SENSOR_WIDTH						640
#define DEFAULT_SENSOR_HEIGHT						480
#define DEFAULT_SENSOR_CODE		MEDIA_BUS_FMT_YUYV8_2X8
#define SENSOR_MEMSIZE	(DEFAULT_SENSOR_WIDTH * DEFAULT_SENSOR_HEIGHT)

#define V4L2_CID_CAM_CONTRAST           (V4L2_CID_CAMERA_CLASS_BASE+42)
enum v4l2_contrast {
	V4L2_CONTRAST_AUTO,
	V4L2_CONTRAST_MINUS_4,
	V4L2_CONTRAST_MINUS_3,
	V4L2_CONTRAST_MINUS_2,
	V4L2_CONTRAST_MINUS_1,
	V4L2_CONTRAST_DEFAULT,
	V4L2_CONTRAST_PLUS_1,
	V4L2_CONTRAST_PLUS_2,
	V4L2_CONTRAST_PLUS_3,
	V4L2_CONTRAST_PLUS_4,
	V4L2_CONTRAST_MAX
};

#define V4L2_CID_CAM_SATURATION         (V4L2_CID_CAMERA_CLASS_BASE+43)
enum v4l2_saturation {
	V4L2_SATURATION_MINUS_3,
	V4L2_SATURATION_MINUS_2,
	V4L2_SATURATION_MINUS_1,
	V4L2_SATURATION_DEFAULT,
	V4L2_SATURATION_PLUS_1,
	V4L2_SATURATION_PLUS_2,
	V4L2_SATURATION_PLUS_3,
	V4L2_SATURATION_MAX
};

#define V4L2_CID_CAM_SHARPNESS          (V4L2_CID_CAMERA_CLASS_BASE+44)
enum v4l2_sharpness {
	V4L2_SHARPNESS_MINUS_2,
	V4L2_SHARPNESS_MINUS_1,
	V4L2_SHARPNESS_DEFAULT,
	V4L2_SHARPNESS_PLUS_1,
	V4L2_SHARPNESS_PLUS_2,
	V4L2_SHARPNESS_MAX,
};

#define V4L2_CID_CAM_ISO    (V4L2_CID_CAMERA_CLASS_BASE+22)
enum v4l2_iso {
	V4L2_ISO_AUTO,
	V4L2_ISO_50,
	V4L2_ISO_100,
	V4L2_ISO_200,
	V4L2_ISO_400,
	V4L2_ISO_800,
	V4L2_ISO_1600,
	V4L2_ISO_MAX
};

#define V4L2_CID_CAPTURE			(V4L2_CID_CAMERA_CLASS_BASE+46)

#define V4L2_CID_CAM_FLASH_MODE			(V4L2_CID_CAMERA_CLASS_BASE+62)
enum v4l2_cam_flash_mode {
	V4L2_FLASH_MODE_BASE,
	V4L2_FLASH_MODE_OFF,
	V4L2_FLASH_MODE_AUTO,
	V4L2_FLASH_MODE_ON,
	V4L2_FLASH_MODE_TORCH,
	V4L2_FLASH_MODE_MAX,
};

#define V4L2_CID_JPEG_QUALITY			(V4L2_CID_CAMERA_CLASS_BASE+55)

#define V4L2_CID_CAM_OBJECT_POSITION_X		(V4L2_CID_CAMERA_CLASS_BASE+50)
#define V4L2_CID_CAM_OBJECT_POSITION_Y		(V4L2_CID_CAMERA_CLASS_BASE+51)

#define V4L2_CID_CAM_SET_AUTO_FOCUS		(V4L2_CID_CAMERA_CLASS_BASE+48)
enum v4l2_cam_auto_focus {
	V4L2_AUTO_FOCUS_OFF = 0,
	V4L2_AUTO_FOCUS_ON,
	V4L2_AUTO_FOCUS_MAX,
};

#define V4L2_CID_CAM_SINGLE_AUTO_FOCUS		(V4L2_CID_CAMERA_CLASS_BASE+63)

#define V4L2_CID_CAM_AUTO_FOCUS_RESULT		(V4L2_CID_CAMERA_CLASS_BASE+54)
enum v4l2_cam_af_status {
	V4L2_CAMERA_AF_STATUS_IN_PROGRESS = 0,
	V4L2_CAMERA_AF_STATUS_SUCCESS,
	V4L2_CAMERA_AF_STATUS_FAIL,
	V4L2_CAMERA_AF_STATUS_1ST_SUCCESS,
	V4L2_CAMERA_AF_STATUS_MAX,
};

#define V4L2_CID_CAM_BRIGHTNESS			(V4L2_CID_CAMERA_CLASS_BASE+45)
enum v4l2_brightness {
	V4L2_BRIGHTNESS_MINUS_2,
	V4L2_BRIGHTNESS_MINUS_1,
	V4L2_BRIGHTNESS_DEFAULT,
	V4L2_BRIGHTNESS_PLUS_1,
	V4L2_BRIGHTNESS_PLUS_2,
	V4L2_BRIGHTNESS_MAX,
};


struct i2c_client *g_s5k5ea_i2c_client;

struct fimc_is_sensor_cfg config_5ea[] = {
	FIMC_IS_SENSOR_CFG(640, 480, 30, 12, 0),
	FIMC_IS_SENSOR_CFG(1280, 720, 30, 12, 1),
	FIMC_IS_SENSOR_CFG(1920, 1080, 30, 12, 2),
};

struct s5k5ea_regset {
	u32 size;
	u8 *data;
};

struct s5k5ea_userset {
	int camera_mode;
	int frame_mode;
	int scenemode;
	int brightness;
	int white_balance;
	int contrast;
	int saturation;
	int sharpness;
	int iso;
	int framerate;
};

struct afpoint {
	u16 x;
	u16 y;
};

struct resolution {
	u32 width;
	u32 height;
	u32 code;
};

struct s5k5ea_reg_item {
	u16 addr;
	u16 data;
};


struct s5k5ea_state {
	struct s5k5ea_platform_data     *pdata;
	struct media_pad                pad;    /* for media deivce pad */
	struct v4l2_subdev              sd;
	struct s5k5ea_userset           userset;
	struct resolution               size;
	struct afpoint                  point;
	struct mutex                    lock;
	struct mutex                    i2c_lock;
	u32								preset_index;
};

struct s5k5ea_framesize {
	u32 index;
	u32 width;
	u32 height;
};

enum s5k5ea_oprmode {
	S5K5EA_OPRMODE_VIDEO = 0,
	S5K5EA_OPRMODE_IMAGE = 1,
	S5K5EA_OPRMODE_MAX,
};

enum s5k5ea_frame_mode {
	FM_LOW      = 0,
	FM_NORMAL   = 1,
	FM_HIGH     = 2,
	FM_MAX,
};

enum s5k5ea_preview_size {
	S5K5EA_PREVIEW_1280_960 = 0,
	S5K5EA_PREVIEW_1920_1080,
	S5K5EA_PREVIEW_1280_720,
	S5K5EA_PREVIEW_720_480,
	S5K5EA_PREVIEW_640_480,
	S5K5EA_PREVIEW_MAX,
};

enum s5k5ea_capture_size {
	S5K5EA_CAPTURE_2560_1920 = 0,
	S5K5EA_CAPTURE_2048_1536,
	S5K5EA_CAPTURE_1600_1200,
	S5K5EA_CAPTURE_1024_768,
	S5K5EA_CAPTURE_640_480,
	S5K5EA_CAPTURE_MAX,
};

enum s5k5ea_preview_input {
	S5K5EA_PREVIEW_INPUT_2560_1920 = 0,
	S5K5EA_PREVIEW_INPUT_1920_1080,
	S5K5EA_PREVIEW_INPUT_2560_1440,
	S5K5EA_PREVIEW_INPUT_MAX,
};

#define EV_MIN_VALUE  EV_MINUS_2
#define EV_MAX_VALUE  EV_PLUS_2
#define GET_EV_INDEX(val, min) ((val) - (min))

static struct v4l2_mbus_framefmt default_fmt[S5K5EA_OPRMODE_MAX] = {
	[S5K5EA_OPRMODE_VIDEO] = {
		.width      = DEFAULT_SENSOR_WIDTH,
		.height     = DEFAULT_SENSOR_HEIGHT,
		.code       = MEDIA_BUS_FMT_YUYV8_2X8,
		.field      = V4L2_FIELD_NONE,
		.colorspace = V4L2_COLORSPACE_JPEG,
	},
	[S5K5EA_OPRMODE_IMAGE] = {
		.width      = DEFAULT_SENSOR_WIDTH,
		.height     = DEFAULT_SENSOR_HEIGHT,
		.code       = MEDIA_BUS_FMT_JPEG_1X8,
		.field      = V4L2_FIELD_NONE,
		.colorspace = V4L2_COLORSPACE_JPEG,
	},
};

#define SIZE_DEFAULT_FFMT   ARRAY_SIZE(default_fmt)

struct s5k5ea_regset_table {
	const struct s5k5ea_reg_item	*reg;
	const char	*setting_name;
	int		array_size;
};

#define S5K5EA_REGSET_TABLE(y)			\
	{						\
		.reg		= s5k5ea_##y,	\
		.setting_name	= "s5k5ea_"#y,	\
		.array_size	= ARRAY_SIZE(s5k5ea_##y),	\
	}

struct s5k5ea_regs {
	struct s5k5ea_regset_table init_reg_1;
	struct s5k5ea_regset_table init_reg_2;
	struct s5k5ea_regset_table frame_time[FM_MAX];
	struct s5k5ea_regset_table scene_mode[V4L2_SCENE_MODE_TEXT+1];
	struct s5k5ea_regset_table wb[WHITE_BALANCE_MAX];
	struct s5k5ea_regset_table preview_input[S5K5EA_PREVIEW_INPUT_MAX];
};

struct fimc_is_module_5ea {
	u16		vis_duration;
	u16		frame_length_line;
	u32		line_length_pck;
	u32		system_clock;
};

int sensor_5ea_probe(struct i2c_client *client,
	const struct i2c_device_id *id);

#endif
