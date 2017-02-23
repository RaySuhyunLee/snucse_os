/* linux/drivers/video/backlight/shiri_mipi_lcd.c
 *
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
*/

#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/backlight.h>

#include <video/mipi_display.h>
#include "decon_mipi_dsi.h"
#include "decon_display_driver.h"

enum {
    MIPI_MCS_PASSWD1            = 0xF0,
    MIPI_MCS_PASSWD2            = 0xF1,
    MIPI_MCS_DISPCTL            = 0xF2,
    MIPI_MCS_MAINPWRSEQ     	= 0xF3,
    MIPI_MCS_PWRCTL         	= 0xF4,
    MIPI_MCS_VCMCTL         	= 0xF5,
    MIPI_MCS_SRCCTL         	= 0xF6,
    MIPI_MCS_IFCTL          	= 0xF7,
    MIPI_MCS_PANELCTL           = 0xF8,
    MIPI_MCS_IFCLK_CTL      	= 0xF9,
    MIPI_MCS_GAMMACTL           = 0xFA,
    MIPI_MCS_PASSWD3            = 0xFC,
    MIPI_MCS_MTPMVCTL           = 0xFE,
};

#if 1
	#define gprintk(fmt, x... ) printk( "%s: " fmt, __FUNCTION__ , ## x)
#else
	#define gprintk(x...) do { } while (0)
#endif

struct decon_lcd shiri_mipi_lcd_info = {
        .mode = COMMAND_MODE,

        .vfp = 1,
        .vbp = 1,
        .hfp = 1,
        .hbp = 1,

        .vsa = 1,
        .hsa = 1,

        .xres = 240,
        .yres = 240,

        .width = 24,
        .height = 24,

        /* Mhz */
        .hs_clk = 100,
        .esc_clk = 7,

        .fps = 60,
};

struct decon_lcd * decon_get_lcd_info()
{
        return &shiri_mipi_lcd_info;
}

static void lh154q01_mipi_lcd_display_on(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM, MIPI_DCS_SET_DISPLAY_ON, 0x00);
	schedule_timeout(msecs_to_jiffies(100));
}

static void lh154q01_mipi_lcd_display_off(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM, MIPI_DCS_SET_DISPLAY_OFF, 0x00);
	schedule_timeout(msecs_to_jiffies(100));
}


static void lh154q01_power_sequence_on(struct mipi_dsim_device *dsim)
{
	unsigned char MCS_CMD_01[7]	= {0xE1, 0xF3, 0x10, 0x1C, 0x17, 0x08, 0x1D};
	unsigned char MCS_CMD_02[18]	= {0xF2, 0x00, 0xD7, 0x03, 0x22, 0x23, 0x00, 0x01, 0x01, 0x12, 0x01, 0x08, 0x57, 0x00, 0x00, 0xD7, 0x22, 0x23 };
	unsigned char MCS_CMD_03[15]	= {0xF4, 0x0B, 0x00, 0x00, 0x00, 0x21, 0x4F, 0x01, 0x02, 0x2A, 0x7F, 0x03, 0x2A, 0x00, 0x03 };
	unsigned char MCS_CMD_04[11]	= {0xF5, 0x00, 0x30, 0x49, 0x00, 0x00, 0x18, 0x00, 0x00, 0x04, 0x04 };
	unsigned char MCS_CMD_05[10]	= {0xF6, 0x02, 0x01, 0x06, 0x00, 0x02, 0x04, 0x02, 0x84, 0x06 };
	unsigned char MCS_CMD_06[2]	= {0xF7, 0x40 };
	unsigned char MCS_CMD_07[3]	= {0xF8, 0x33, 0x00 };
	unsigned char MCS_CMD_08[2]	= {0xF9, 0x00 };
	unsigned char MCS_CMD_09[2]	= {0x36, 0x08 };

	unsigned char MCS_CMD_E1[7]	= {0xE1, 0xF3, 0x10, 0x1C, 0x17, 0x08, 0x1D };
	unsigned char MCS_CMD_E2[6]	= {0xE2, 0xC3, 0x87, 0x39, 0x63, 0xD5 };
	unsigned char MCS_CMD_E3[4]	= {0xE3, 0x84, 0x06, 0x52 };
	unsigned char MCS_CMD_E4[4]	= {0xE4, 0x43, 0x00, 0x00 };

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,(unsigned int) MCS_CMD_01, 	7);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,(unsigned int) MCS_CMD_02, 	18);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,(unsigned int) MCS_CMD_03, 	15);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,(unsigned int) MCS_CMD_04, 	11);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,(unsigned int) MCS_CMD_05, 	10);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,(unsigned int) MCS_CMD_06, 	2);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,(unsigned int) MCS_CMD_07, 	3);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,(unsigned int) MCS_CMD_08, 	2);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,(unsigned int) MCS_CMD_09, 	2);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,(unsigned int) MCS_CMD_E1, 	7);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,(unsigned int) MCS_CMD_E2, 	6);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,(unsigned int) MCS_CMD_E3, 	4);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,(unsigned int) MCS_CMD_E4, 	4);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM, MIPI_DCS_EXIT_SLEEP_MODE, 0x00);		
	mdelay(160);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM, MIPI_DCS_SET_ADDRESS_MODE, 0x08);	
}

static void lh154q01_power_sequence_off(struct mipi_dsim_device *dsim)
{
	unsigned char MCS_CMD_F1[3] = {MIPI_MCS_PASSWD2, 0x5A, 0x5A};
	unsigned char MCS_CMD_F1_2[3] = {MIPI_MCS_PASSWD2, 0xA5, 0xA5};
	unsigned char MCS_CMD_F4[15] = {MIPI_MCS_PWRCTL, 0x07, 0x00, 0x00, 0x00, 0x21, 0x4F, 0x01, 0x0E, 0x2A, 0x66, 0x02, 0x2A, 0x00, 0x02};

	mdelay(1);

	lh154q01_mipi_lcd_display_off(dsim);

	mdelay(5);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,(unsigned int) MCS_CMD_F1, 3);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,(unsigned int) MCS_CMD_F4, 15);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,(unsigned int) MCS_CMD_F1_2, 3);

	mdelay(120);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM, MIPI_DCS_ENTER_SLEEP_MODE, 0x00);
}

static int init_lcd(struct mipi_dsim_device *dsim)
{
	lh154q01_power_sequence_on(dsim);
	lh154q01_mipi_lcd_display_on(dsim);

	return 1;
}

void shiri_mipi_lcd_off(struct mipi_dsim_device *dsim)
{
	lh154q01_power_sequence_off(dsim);
	mdelay(1);
}

static int shiri_mipi_lcd_suspend(struct mipi_dsim_device *dsim)
{
	lh154q01_power_sequence_off(dsim);

	return 1;
}

static int shiri_mipi_lcd_displayon(struct mipi_dsim_device *dsim)
{
	init_lcd(dsim);

	return 1;
}

static int shiri_mipi_lcd_resume(struct mipi_dsim_device *dsim)
{
	return 1;
}

int shiri_mipi_lcd_probe(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver shiri_mipi_lcd_driver = {
	.probe   =  shiri_mipi_lcd_probe,
	.suspend =  shiri_mipi_lcd_suspend,
	.displayon = shiri_mipi_lcd_displayon,
	.resume = shiri_mipi_lcd_resume,
};
