/* linux/drivers/video/samsung/s3cfb_ek070tn93.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * LTE480 4.8" WVGA Landscape LCD module driver for the SMDK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "s3cfb.h"

/* EK070TN93 */
static struct s3cfb_lcd ek070tn93 = {
	.width	= 800,
	.height	= 480,
	.bpp	= 32,
	.freq	= 60,

	.timing = {
		.h_fp	= 210,
		.h_bp	= 38,
		.h_sw	= 10,
		.v_fp	= 22,
		.v_fpe	= 1,
		.v_bp	= 18,
		.v_bpe	= 1,
		.v_sw	= 7,
	},

	.polarity = {
		.rise_vclk	= 0,
		.inv_hsync	= 1,
		.inv_vsync	= 1,
		.inv_vden	= 0,
	},

	.init_ldi = NULL,
};

/* VS070CXN */
static struct s3cfb_lcd vs070cxn = {
	.width	= 1024,
	.height	= 600,
	.bpp	= 32,
	.freq	= 60,

	.timing = {
		.h_fp	= 160,
		.h_bp	= 140,
		.h_sw	= 20,
		.v_fp	= 12,
		.v_fpe	= 1,
		.v_bp	= 20,
		.v_bpe	= 1,
		.v_sw	= 3,
	},

	.polarity = {
		.rise_vclk	= 0,
		.inv_hsync	= 1,
		.inv_vsync	= 1,
		.inv_vden	= 0,
	},

	.init_ldi = NULL,
};

/* VGA-1024X768 */
static struct s3cfb_lcd vga_1024_768 = {
	.width	= 1024,
	.height	= 768,
	.bpp	= 32,
	.freq	= 60,

	.timing = {
		.h_fp	= 24,
		.h_bp	= 160,
		.h_sw	= 136,
		.v_fp	= 3,
		.v_fpe	= 1,
		.v_bp	= 29,
		.v_bpe	= 1,
		.v_sw	= 6,
	},

	.polarity = {
		.rise_vclk      = 1,
		.inv_hsync      = 0,
		.inv_vsync      = 0,
		.inv_vden       = 0,
	},
	
	.init_ldi = NULL,
};

/* VGA-1440X900 */
static struct s3cfb_lcd vga_1440_900 = {
	.width  = 1440,
	.height = 900,
	.bpp    = 32,
	.freq   = 60,

	.timing = {
		 .h_fp   = 48,
		 .h_bp   = 80,
		 .h_sw   = 32,
		 .v_fp   = 3,
		 .v_fpe  = 1,
		 .v_bp   = 17,
		 .v_bpe  = 1,
		 .v_sw   = 6,
	},

	.polarity = {
		 .rise_vclk		= 1,
		 .inv_hsync		= 1,
		 .inv_vsync		= 0,
		 .inv_vden		= 0,
	},

	.init_ldi = NULL,
};

/* VGA-1280X1024 */
static struct s3cfb_lcd vga_1280_1024 = {
	.width	= 1280,
	.height	= 1024,
	.bpp	= 32,
	.freq	= 60,

	.timing = {
		.h_fp	= 48,
		.h_bp	= 248,
		.h_sw	= 112,
		.v_fp	= 1,
		.v_fpe	= 1,
		.v_bp	= 38,
		.v_bpe	= 1,
		.v_sw	= 3,
	},
	
	.polarity = {
		.rise_vclk      = 1,
		.inv_hsync      = 1,
		.inv_vsync      = 1,
		.inv_vden       = 0,
	},
	
	.init_ldi = NULL,
};

/* VGA-1920X1200 */
static struct s3cfb_lcd vga_1920_1200 = {
	.width	= 1920,
	.height	= 1200,
	.bpp	= 32,
	.freq	= 60,

	.timing = {
		.h_fp	= 48,
		.h_bp	= 80,
		.h_sw	= 32,
		.v_fp	= 3,
		.v_fpe	= 1,
		.v_bp	= 26,
		.v_bpe	= 1,
		.v_sw	= 6,
	},
	
	.polarity = {
		.rise_vclk      = 1,
		.inv_hsync      = 1,
		.inv_vsync      = 0,
		.inv_vden       = 0,
	},

	.init_ldi = NULL,
};

static struct {
	char * name;
	struct s3cfb_lcd * lcd;
} x4412_lcd_config[] = {
	{ "ek070tn93",		&ek070tn93},
	{ "vs070cxn",		&vs070cxn},
	{ "vga-1024x768",	&vga_1024_768},
	{ "vga-1440x900",	&vga_1440_900},
	{ "vga-1280x1024",	&vga_1280_1024},
	{ "vga-1920x1200",	&vga_1920_1200},
};

static unsigned char lcd_name[32] = "ek070tn93";
static int __init lcd_setup(char * str)
{
    if((str != NULL) && (*str != '\0'))
    	strcpy(lcd_name, str);
	return 1;
}
__setup("lcd=", lcd_setup);

void s3cfb_set_lcd_info(struct s3cfb_global *ctrl)
{
	struct s3cfb_lcd * lcd = x4412_lcd_config[0].lcd;
	int i;

	for(i = 0; i < ARRAY_SIZE(x4412_lcd_config); i++)
	{
		if(strcasecmp(x4412_lcd_config[i].name, lcd_name) == 0)
		{
			lcd = x4412_lcd_config[i].lcd;
			break;
		}
	}
	ctrl->lcd = lcd;
	printk("lcd: select %s\r\n", lcd_name);
}

