/*
 * exynos4412-fb.c
 *
 * Copyright(c) 2007-2014 Jianjun Jiang <8192542@qq.com>
 * Official site: http://xboot.org
 * Mobile phone: +86-18665388956
 * QQ: 8192542
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "types.h"
#include "stddef.h"
#include "stdarg.h"
#include "malloc.h"
#include "io.h"
#include "surface.h"
#include "exynos4412/reg-gpio.h"
#include "exynos4412/reg-others.h"
#include "exynos4412/reg-lcd.h"
#include "exynos4412-clk.h"
#include "exynos4412-fb.h"

enum exynos4412_fb_output_t
{
	EXYNOS4412_FB_OUTPUT_RGB,
	EXYNOS4412_FB_OUTPUT_I80LDI0,
	EXYNOS4412_FB_OUTPUT_I80LDI1,
	EXYNOS4412_FB_OUTPUT_WB_RGB,
	EXYNOS4412_FB_OUTPUT_WB_I80LDI0,
	EXYNOS4412_FB_OUTPUT_WB_I80LDI1,
};

enum exynos4412_fb_rgb_mode_t
{
	EXYNOS4412_FB_MODE_RGB_P 	= 0,
	EXYNOS4412_FB_MODE_BGR_P 	= 1,
	EXYNOS4412_FB_MODE_RGB_S 	= 2,
	EXYNOS4412_FB_MODE_BGR_S 	= 3,
};

enum exynos4412_bpp_mode_t
{
	EXYNOS4412_FB_BPP_MODE_1BPP			= 0x0,
	EXYNOS4412_FB_BPP_MODE_2BPP			= 0x1,
	EXYNOS4412_FB_BPP_MODE_4BPP			= 0x2,
	EXYNOS4412_FB_BPP_MODE_8BPP_PAL		= 0x3,
	EXYNOS4412_FB_BPP_MODE_8BPP			= 0x4,
	EXYNOS4412_FB_BPP_MODE_16BPP_565	= 0x5,
	EXYNOS4412_FB_BPP_MODE_16BPP_A555	= 0x6,
	EXYNOS4412_FB_BPP_MODE_16BPP_X555	= 0x7,
	EXYNOS4412_FB_BPP_MODE_18BPP_666	= 0x8,
	EXYNOS4412_FB_BPP_MODE_18BPP_A665	= 0x9,
	EXYNOS4412_FB_BPP_MODE_19BPP_A666	= 0xa,
	EXYNOS4412_FB_BPP_MODE_24BPP_888	= 0xb,
	EXYNOS4412_FB_BPP_MODE_24BPP_A887	= 0xc,
	EXYNOS4412_FB_BPP_MODE_32BPP		= 0xd,
	EXYNOS4412_FB_BPP_MODE_16BPP_A444	= 0xe,
	EXYNOS4412_FB_BPP_MODE_15BPP_555	= 0xf,
};

enum {
	EXYNOS4412_FB_SWAP_WORD		= (0x1 << 0),
	EXYNOS4412_FB_SWAP_HWORD	= (0x1 << 1),
	EXYNOS4412_FB_SWAP_BYTE		= (0x1 << 2),
	EXYNOS4412_FB_SWAP_BIT		= (0x1 << 3),
};

struct exynos4412_fb_data_t
{
	/* Register base address */
	unsigned int regbase;

	/* horizontal resolution */
	s32_t width;

	/* vertical resolution */
	s32_t height;

	/* Bits per pixel */
	s32_t bits_per_pixel;

	/* Bytes per pixel */
	s32_t bytes_per_pixel;

	/* Vframe frequency */
	s32_t freq;

	/* Output path */
	enum exynos4412_fb_output_t output;

	/* RGB mode */
	enum exynos4412_fb_rgb_mode_t rgb_mode;

	/* bpp mode */
	enum exynos4412_bpp_mode_t bpp_mode;

	/* Swap flag */
	u32_t swap;

	struct {
		/* red color */
		s32_t r_mask;
		s32_t r_field;

		/* green color */
		s32_t g_mask;
		s32_t g_field;

		/* blue color */
		s32_t b_mask;
		s32_t b_field;

		/* alpha color */
		s32_t a_mask;
		s32_t a_field;
	} rgba;

	struct {
		/* horizontal front porch */
		s32_t h_fp;

		/* horizontal back porch */
		s32_t h_bp;

		/* horizontal sync width */
		s32_t h_sw;

		/* vertical front porch */
		s32_t v_fp;

		/* vertical front porch for even field */
		s32_t v_fpe;

		/* vertical back porch */
		s32_t v_bp;

		/* vertical back porch for even field */
		s32_t v_bpe;

		/* vertical sync width */
		s32_t v_sw;
	} timing;

	struct {
		/* if 1, video data is fetched at rising edge */
		s32_t rise_vclk;

		/* if HSYNC polarity is inversed */
		s32_t inv_hsync;

		/* if VSYNC polarity is inversed */
		s32_t inv_vsync;

		/* if VDEN polarity is inversed */
		s32_t inv_vden;
	} polarity;

	/* video ram front buffer */
	void * vram_front;

	/* video ram back buffer */
	void * vram_back;

	/* lcd init */
	void (*init)(void);

	/* lcd backlight */
	void (*backlight)(u8_t brightness);
};

/*
 * defined the structure of framebuffer.
 */
struct fb_t
{
	/* the framebuffer's surface */
	struct surface_t surface;

	/* framebuffer's lcd data */
	struct exynos4412_fb_data_t * dat;
};

static bool_t exynos4412_fb_set_output(struct exynos4412_fb_data_t * dat)
{
	u32_t cfg;

	cfg = readl(dat->regbase + VIDCON0);
	cfg &= ~VIDCON0_VIDOUT_MASK;

	if(dat->output == EXYNOS4412_FB_OUTPUT_RGB)
		cfg |= VIDCON0_VIDOUT_RGB;
	else if(dat->output == EXYNOS4412_FB_OUTPUT_I80LDI0)
		cfg |= VIDCON0_VIDOUT_I80LDI0;
	else if(dat->output == EXYNOS4412_FB_OUTPUT_I80LDI1)
		cfg |= VIDCON0_VIDOUT_I80LDI1;
	else if(dat->output == EXYNOS4412_FB_OUTPUT_WB_RGB)
		cfg |= VIDCON0_VIDOUT_WB_RGB;
	else if(dat->output == EXYNOS4412_FB_OUTPUT_WB_I80LDI0)
		cfg |= VIDCON0_VIDOUT_WB_I80LDI0;
	else if(dat->output == EXYNOS4412_FB_OUTPUT_WB_I80LDI1)
		cfg |= VIDCON0_VIDOUT_WB_I80LDI1;
	else
		return FALSE;
	writel(dat->regbase + VIDCON0, cfg);

	cfg = readl(dat->regbase + VIDCON2);
	cfg |= 0x3 << 14;
	writel(dat->regbase + VIDCON2, cfg);

	return TRUE;
}

static bool_t exynos4412_fb_set_display_mode(struct exynos4412_fb_data_t * dat)
{
	u32_t cfg;

	cfg = readl(dat->regbase + VIDCON0);
	cfg &= ~(3 << 17);
	cfg |= (dat->rgb_mode << 17);
	writel(dat->regbase + VIDCON0, cfg);

	return TRUE;
}

static bool_t exynos4412_fb_display_on(struct exynos4412_fb_data_t * dat)
{
	u32_t cfg;

	cfg = readl(dat->regbase + VIDCON0);
	cfg |= 0x3 << 0;
	writel(dat->regbase + VIDCON0, cfg);

	return TRUE;
}

static bool_t exynos4412_fb_display_off(struct exynos4412_fb_data_t * dat)
{
	u32_t cfg;

	cfg = readl(dat->regbase + VIDCON0);
	cfg &= ~(1 << 1);
	writel(dat->regbase + VIDCON0, cfg);

	cfg &= ~(1 << 0);
	writel(dat->regbase + VIDCON0, cfg);

	return TRUE;
}

static bool_t exynos4412_fb_set_clock(struct exynos4412_fb_data_t * dat)
{
	u64_t fimd, pixel_clock;
	u32_t div;
	u32_t cfg;

	if(!clk_get_rate("fimd", &fimd))
		return FALSE;

	pixel_clock = ( dat->freq * (dat->timing.h_fp + dat->timing.h_bp + dat->timing.h_sw + dat->width) *
			(dat->timing.v_fp + dat->timing.v_bp + dat->timing.v_sw + dat->height) );

	div = (u32_t)(fimd / pixel_clock);
	if((fimd % pixel_clock) > 0)
		div++;

	cfg = readl(dat->regbase + VIDCON0);
	cfg &= ~( (1 << 16) | (1 << 5) );
	cfg |= VIDCON0_CLKVAL_F(div - 1);
	writel(dat->regbase + VIDCON0, cfg);

	return TRUE;
}

static bool_t exynos4412_fb_set_polarity(struct exynos4412_fb_data_t * dat)
{
	u32_t cfg = 0;

	if(dat->polarity.rise_vclk)
		cfg |= (1 << 7);

	if(dat->polarity.inv_hsync)
		cfg |= (1 << 6);

	if(dat->polarity.inv_vsync)
		cfg |= (1 << 5);

	if(dat->polarity.inv_vden)
		cfg |= (1 << 4);

	writel(dat->regbase + VIDCON1, cfg);
	return TRUE;
}

static bool_t exynos4412_fb_set_timing(struct exynos4412_fb_data_t * dat)
{
	u32_t cfg;

	cfg = 0;
	cfg |= VIDTCON0_VBPDE(dat->timing.v_bpe - 1);
	cfg |= VIDTCON0_VBPD(dat->timing.v_bp - 1);
	cfg |= VIDTCON0_VFPD(dat->timing.v_fp - 1);
	cfg |= VIDTCON0_VSPW(dat->timing.v_sw - 1);
	writel(dat->regbase + VIDTCON0, cfg);

	cfg = 0;
	cfg |= VIDTCON1_VFPDE(dat->timing.v_fpe - 1);
	cfg |= VIDTCON1_HBPD(dat->timing.h_bp - 1);
	cfg |= VIDTCON1_HFPD(dat->timing.h_fp - 1);
	cfg |= VIDTCON1_HSPW(dat->timing.h_sw - 1);
	writel(dat->regbase + VIDTCON1, cfg);

	return TRUE;
}

static bool_t exynos4412_fb_set_lcd_size(struct exynos4412_fb_data_t * dat)
{
	u32_t cfg = 0;

	cfg |= VIDTCON2_HOZVAL(dat->width - 1);
	cfg |= VIDTCON2_LINEVAL(dat->height - 1);
	writel(dat->regbase + VIDTCON2, cfg);

	return TRUE;
}

static bool_t exynos4412_fb_set_buffer_address(struct exynos4412_fb_data_t * dat, s32_t id, void * vram)
{
	u32_t start, end;
	u32_t shw;

	start = (u32_t)(vram);
	end = (u32_t)((start + dat->width * (dat->height * dat->bytes_per_pixel)) & 0x00ffffff);

	shw = readl(dat->regbase + SHADOWCON);
	shw |= SHADOWCON_PROTECT(id);
	writel(dat->regbase + SHADOWCON, shw);

	switch(id)
	{
	case 0:
		writel(dat->regbase + VIDW00ADD0B0, start);
		writel(dat->regbase + VIDW00ADD1B0, end);
		break;

	case 1:
		writel(dat->regbase + VIDW01ADD0B0, start);
		writel(dat->regbase + VIDW01ADD1B0, end);
		break;

	case 2:
		writel(dat->regbase + VIDW02ADD0B0, start);
		writel(dat->regbase + VIDW02ADD1B0, end);
		break;

	case 3:
		writel(dat->regbase + VIDW03ADD0B0, start);
		writel(dat->regbase + VIDW03ADD1B0, end);
		break;

	case 4:
		writel(dat->regbase + VIDW04ADD0B0, start);
		writel(dat->regbase + VIDW04ADD1B0, end);
		break;

	default:
		break;
	}

	shw = readl(dat->regbase + SHADOWCON);
	shw &= ~(SHADOWCON_PROTECT(id));
	writel(dat->regbase + SHADOWCON, shw);

	return TRUE;
}

static bool_t exynos4412_fb_set_buffer_size(struct exynos4412_fb_data_t * dat, s32_t id)
{
	u32_t cfg = 0;

	cfg = VIDADDR_PAGEWIDTH(dat->width * dat->bytes_per_pixel);
	cfg |= VIDADDR_OFFSIZE(0);

	switch(id)
	{
	case 0:
		writel(dat->regbase + VIDW00ADD2, cfg);
		break;

	case 1:
		writel(dat->regbase + VIDW01ADD2, cfg);
		break;

	case 2:
		writel(dat->regbase + VIDW02ADD2, cfg);
		break;

	case 3:
		writel(dat->regbase + VIDW03ADD2, cfg);
		break;

	case 4:
		writel(dat->regbase + VIDW04ADD2, cfg);
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

static bool_t exynos4412_fb_set_window_position(struct exynos4412_fb_data_t * dat, s32_t id)
{
	u32_t cfg, shw;

	shw = readl(dat->regbase + SHADOWCON);
	shw |= SHADOWCON_PROTECT(id);
	writel(dat->regbase + SHADOWCON, shw);

	switch(id)
	{
	case 0:
		cfg = VIDOSD_LEFT_X(0) | VIDOSD_TOP_Y(0);
		writel(dat->regbase + VIDOSD0A, cfg);
		cfg = VIDOSD_RIGHT_X(dat->width - 1) | VIDOSD_BOTTOM_Y(dat->height - 1);
		writel(dat->regbase + VIDOSD0B, cfg);
		break;

	case 1:
		cfg = VIDOSD_LEFT_X(0) | VIDOSD_TOP_Y(0);
		writel(dat->regbase + VIDOSD1A, cfg);
		cfg = VIDOSD_RIGHT_X(dat->width - 1) | VIDOSD_BOTTOM_Y(dat->height - 1);
		writel(dat->regbase + VIDOSD1B, cfg);
		break;

	case 2:
		cfg = VIDOSD_LEFT_X(0) | VIDOSD_TOP_Y(0);
		writel(dat->regbase + VIDOSD2A, cfg);
		cfg = VIDOSD_RIGHT_X(dat->width - 1) | VIDOSD_BOTTOM_Y(dat->height - 1);
		writel(dat->regbase + VIDOSD2B, cfg);
		break;

	case 3:
		cfg = VIDOSD_LEFT_X(0) | VIDOSD_TOP_Y(0);
		writel(dat->regbase + VIDOSD3A, cfg);
		cfg = VIDOSD_RIGHT_X(dat->width - 1) | VIDOSD_BOTTOM_Y(dat->height - 1);
		writel(dat->regbase + VIDOSD3B, cfg);
		break;

	case 4:
		cfg = VIDOSD_LEFT_X(0) | VIDOSD_TOP_Y(0);
		writel(dat->regbase + VIDOSD4A, cfg);
		cfg = VIDOSD_RIGHT_X(dat->width - 1) | VIDOSD_BOTTOM_Y(dat->height - 1);
		writel(dat->regbase + VIDOSD4B, cfg);
		break;

	default:
		break;
	}

	shw = readl(dat->regbase + SHADOWCON);
	shw &= ~(SHADOWCON_PROTECT(id));
	writel(dat->regbase + SHADOWCON, shw);

	return TRUE;
}

static bool_t exynos4412_fb_set_window_size(struct exynos4412_fb_data_t * dat, s32_t id)
{
	u32_t cfg;

	if(id > 2)
		return FALSE;

	cfg = VIDOSD_SIZE(dat->width * dat->height);
	switch(id)
	{
	case 0:
		writel(dat->regbase + VIDOSD0C, cfg);
		break;

	case 1:
		writel(dat->regbase + VIDOSD1D, cfg);
		break;

	case 2:
		writel(dat->regbase + VIDOSD2D, cfg);
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

static bool_t exynos4412_fb_window0_enable(struct exynos4412_fb_data_t * dat)
{
	u32_t cfg;

	cfg = readl(dat->regbase + WINCON0);
	cfg &= ~((1 << 18) | (1 << 17) |
			(1 << 16) | (1 << 15) |
			(3 << 9) | (0xf << 2) |
			(1 << 13) | (1 << 22) |
			(1 << 1));
	cfg |= (0 << 1);
	cfg |= (0 << 13);
	cfg |= (0 << 22);
	cfg |= (1 << 0);

	if(dat->swap & EXYNOS4412_FB_SWAP_WORD)
		cfg |= (1 << 15);

	if(dat->swap & EXYNOS4412_FB_SWAP_HWORD)
		cfg |= (1 << 16);

	if(dat->swap & EXYNOS4412_FB_SWAP_BYTE)
		cfg |= (1 << 17);

	if(dat->swap & EXYNOS4412_FB_SWAP_BIT)
		cfg |= (1 << 18);

	cfg |= (dat->bpp_mode << 2);
	writel(dat->regbase + WINCON0, cfg);

	cfg = readl(dat->regbase + SHADOWCON);
	cfg |= 1 << 0;
	writel(dat->regbase + SHADOWCON, cfg);

	return TRUE;
}

static void lcd_init(void)
{
	/*
	 * Set GPD0_0 (backlight pwm pin) output and pull up and high level for disabled
	 */
	writel(EXYNOS4412_GPD0_BASE + EXYNOS4412_GPIO_CON, (readl(EXYNOS4412_GPD0_BASE + EXYNOS4412_GPIO_CON) & ~(0xf<<0)) | (0x1<<0));
	writel(EXYNOS4412_GPD0_BASE + EXYNOS4412_GPIO_PUD, (readl(EXYNOS4412_GPD0_BASE + EXYNOS4412_GPIO_PUD) & ~(0x3<<0)) | (0x2<<0));
	writel(EXYNOS4412_GPD0_BASE + EXYNOS4412_GPIO_DAT, (readl(EXYNOS4412_GPD0_BASE + EXYNOS4412_GPIO_DAT) & ~(0x1<<0)) | (0x1<<0));

	/*
	 * Set GPX3_5 (backlight enable pin) output and pull up and low level for disabled
	 */
	writel(EXYNOS4412_GPX3_BASE + EXYNOS4412_GPIO_CON, (readl(EXYNOS4412_GPX3_BASE + EXYNOS4412_GPIO_CON) & ~(0xf<<20)) | (0x1<<20));
	writel(EXYNOS4412_GPX3_BASE + EXYNOS4412_GPIO_PUD, (readl(EXYNOS4412_GPX3_BASE + EXYNOS4412_GPIO_PUD) & ~(0x3<<10)) | (0x2<<10));
	writel(EXYNOS4412_GPX3_BASE + EXYNOS4412_GPIO_DAT, (readl(EXYNOS4412_GPX3_BASE + EXYNOS4412_GPIO_DAT) & ~(0x1<<5)) | (0x0<<5));
}

static void lcd_backlight(u8_t brightness)
{
	if(brightness)
	{
		writel(EXYNOS4412_GPD0_BASE + EXYNOS4412_GPIO_DAT, (readl(EXYNOS4412_GPD0_BASE + EXYNOS4412_GPIO_DAT) & ~(0x1<<0)) | (0x0<<0));
		writel(EXYNOS4412_GPX3_BASE + EXYNOS4412_GPIO_DAT, (readl(EXYNOS4412_GPX3_BASE + EXYNOS4412_GPIO_DAT) & ~(0x1<<5)) | (0x1<<5));
	}
	else
	{
		writel(EXYNOS4412_GPD0_BASE + EXYNOS4412_GPIO_DAT, (readl(EXYNOS4412_GPD0_BASE + EXYNOS4412_GPIO_DAT) & ~(0x1<<0)) | (0x1<<0));
		writel(EXYNOS4412_GPX3_BASE + EXYNOS4412_GPIO_DAT, (readl(EXYNOS4412_GPX3_BASE + EXYNOS4412_GPIO_DAT) & ~(0x1<<5)) | (0x0<<5));
	}
}

static void fb_init(struct fb_t * fb)
{
	struct exynos4412_fb_data_t * dat = (struct exynos4412_fb_data_t *)(fb->dat);

	/*
	 * Initial lcd port
	 */
	writel(EXYNOS4412_GPF0_BASE + EXYNOS4412_GPIO_CON, 0x22222222);
	writel(EXYNOS4412_GPF0_BASE + EXYNOS4412_GPIO_DRV, 0xffffffff);
	writel(EXYNOS4412_GPF0_BASE + EXYNOS4412_GPIO_PUD, 0x0);
	writel(EXYNOS4412_GPF1_BASE + EXYNOS4412_GPIO_CON, 0x22222222);
	writel(EXYNOS4412_GPF1_BASE + EXYNOS4412_GPIO_DRV, 0xffffffff);
	writel(EXYNOS4412_GPF1_BASE + EXYNOS4412_GPIO_PUD, 0x0);
	writel(EXYNOS4412_GPF2_BASE + EXYNOS4412_GPIO_CON, 0x22222222);
	writel(EXYNOS4412_GPF2_BASE + EXYNOS4412_GPIO_DRV, 0xffffffff);
	writel(EXYNOS4412_GPF2_BASE + EXYNOS4412_GPIO_PUD, 0x0);
	writel(EXYNOS4412_GPF3_BASE + EXYNOS4412_GPIO_CON, (readl(EXYNOS4412_GPF3_BASE + EXYNOS4412_GPIO_CON) & ~(0xffff<<0)) | (0x2222<<0));
	writel(EXYNOS4412_GPF3_BASE + EXYNOS4412_GPIO_DRV, (readl(EXYNOS4412_GPF3_BASE + EXYNOS4412_GPIO_DRV) & ~(0xff<<0)) | (0xff<<0));
	writel(EXYNOS4412_GPF3_BASE + EXYNOS4412_GPIO_PUD, (readl(EXYNOS4412_GPF3_BASE + EXYNOS4412_GPIO_PUD) & ~(0xff<<0)) | (0x00<<0));

	/*
	 * Lcd init function
	 */
	if(dat->init)
		dat->init();

	/*
	 * Display path selection
	 */
	writel(EXYNOS4412_LCDBLK_CFG, (readl(EXYNOS4412_LCDBLK_CFG) & ~(0x3<<0)) | (0x2<<0));
	writel(EXYNOS4412_LCDBLK_CFG2, (readl(EXYNOS4412_LCDBLK_CFG2) & ~(0x1<<0)) | (0x1<<0));

	/*
	 * Turn off all windows
	 */
	writel(dat->regbase + WINCON0, (readl(dat->regbase + WINCON0) & ~0x1));
	writel(dat->regbase + WINCON1, (readl(dat->regbase + WINCON1) & ~0x1));
	writel(dat->regbase + WINCON2, (readl(dat->regbase + WINCON2) & ~0x1));
	writel(dat->regbase + WINCON3, (readl(dat->regbase + WINCON3) & ~0x1));
	writel(dat->regbase + WINCON4, (readl(dat->regbase + WINCON4) & ~0x1));

	/*
	 * Turn off all windows color map
	 */
	writel(dat->regbase + WIN0MAP, (readl(dat->regbase + WIN0MAP) & ~(1<<24)));
	writel(dat->regbase + WIN1MAP, (readl(dat->regbase + WIN1MAP) & ~(1<<24)));
	writel(dat->regbase + WIN2MAP, (readl(dat->regbase + WIN2MAP) & ~(1<<24)));
	writel(dat->regbase + WIN3MAP, (readl(dat->regbase + WIN3MAP) & ~(1<<24)));
	writel(dat->regbase + WIN4MAP, (readl(dat->regbase + WIN4MAP) & ~(1<<24)));

	/*
	 * Turn off all windows color key and blending
	 */
	writel(dat->regbase + W1KEYCON0, (readl(dat->regbase + W1KEYCON0) & ~(3<<25)));
	writel(dat->regbase + W2KEYCON0, (readl(dat->regbase + W2KEYCON0) & ~(3<<25)));
	writel(dat->regbase + W3KEYCON0, (readl(dat->regbase + W3KEYCON0) & ~(3<<25)));
	writel(dat->regbase + W4KEYCON0, (readl(dat->regbase + W4KEYCON0) & ~(3<<25)));

	/*
	 * Initial lcd controller
	 */
	exynos4412_fb_set_output(dat);
	exynos4412_fb_set_display_mode(dat);
	exynos4412_fb_display_off(dat);
	exynos4412_fb_set_polarity(dat);
	exynos4412_fb_set_timing(dat);
	exynos4412_fb_set_lcd_size(dat);
	exynos4412_fb_set_clock(dat);

	/*
	 * Set lcd video buffer
	 */
	exynos4412_fb_set_buffer_size(dat, 0);
	exynos4412_fb_set_window_position(dat, 0);
	exynos4412_fb_set_window_size(dat, 0);

	/*
	 * Enable window 0 for main display area
	 */
	exynos4412_fb_window0_enable(dat);

	/*
	 * Display on
	 */
	exynos4412_fb_display_on(dat);

	/*
	 * Wait a moment
	 */
//	mdelay(100);
}

static void fb_swap(struct fb_t * fb)
{
	struct exynos4412_fb_data_t * dat = (struct exynos4412_fb_data_t *)(fb->dat);
	void * vram;

	vram = dat->vram_front;
	dat->vram_front = dat->vram_back;
	dat->vram_back = vram;

	fb->surface.pixels = dat->vram_front;
}

static void fb_flush(struct fb_t * fb)
{
	struct exynos4412_fb_data_t * dat = (struct exynos4412_fb_data_t *)(fb->dat);

	exynos4412_fb_set_buffer_address(dat, 0, dat->vram_front);
}

static void fb_backlight(struct fb_t * fb, u8_t brightness)
{
	struct exynos4412_fb_data_t * dat = (struct exynos4412_fb_data_t *)(fb->dat);

	if(dat->backlight)
		dat->backlight(brightness);
}

static u8_t vram[2][1920 * 1200 * 32 / 8] __attribute__((aligned(4)));

static struct exynos4412_fb_data_t vs070cxn = {
	.regbase			= EXYNOS4412_LCD_BASE,

	.width				= 1024,
	.height				= 600,
	.bits_per_pixel		= 32,
	.bytes_per_pixel	= 4,
	.freq				= 60,

	.output				= EXYNOS4412_FB_OUTPUT_RGB,
	.rgb_mode			= EXYNOS4412_FB_MODE_BGR_P,
	.bpp_mode			= EXYNOS4412_FB_BPP_MODE_32BPP,
	.swap				= EXYNOS4412_FB_SWAP_WORD,

	.rgba = {
		.r_mask			= 8,
		.r_field		= 0,
		.g_mask			= 8,
		.g_field		= 8,
		.b_mask			= 8,
		.b_field		= 16,
		.a_mask			= 8,
		.a_field		= 24,
	},

	.timing = {
		.h_fp			= 160,
		.h_bp			= 140,
		.h_sw			= 20,
		.v_fp			= 12,
		.v_fpe			= 1,
		.v_bp			= 20,
		.v_bpe			= 1,
		.v_sw			= 3,
	},

	.polarity = {
		.rise_vclk		= 0,
		.inv_hsync		= 1,
		.inv_vsync		= 1,
		.inv_vden		= 0,
	},

	.vram_front			= &vram[0][0],
	.vram_back			= &vram[1][0],

	.init				= lcd_init,
	.backlight			= lcd_backlight,
};

static struct exynos4412_fb_data_t ek070tn93 = {
	.regbase			= EXYNOS4412_LCD_BASE,

	.width				= 800,
	.height				= 480,
	.bits_per_pixel		= 32,
	.bytes_per_pixel	= 4,
	.freq				= 60,

	.output				= EXYNOS4412_FB_OUTPUT_RGB,
	.rgb_mode			= EXYNOS4412_FB_MODE_BGR_P,
	.bpp_mode			= EXYNOS4412_FB_BPP_MODE_32BPP,
	.swap				= EXYNOS4412_FB_SWAP_WORD,

	.rgba = {
		.r_mask			= 8,
		.r_field		= 0,
		.g_mask			= 8,
		.g_field		= 8,
		.b_mask			= 8,
		.b_field		= 16,
		.a_mask			= 8,
		.a_field		= 24,
	},

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
		.rise_vclk		= 0,
		.inv_hsync		= 1,
		.inv_vsync		= 1,
		.inv_vden		= 0,
	},

	.vram_front			= &vram[0][0],
	.vram_back			= &vram[1][0],

	.init				= lcd_init,
	.backlight			= lcd_backlight,
};

static struct exynos4412_fb_data_t vga_1024_768 = {
	.regbase			= EXYNOS4412_LCD_BASE,

	.width				= 1024,
	.height				= 768,
	.bits_per_pixel		= 32,
	.bytes_per_pixel	= 4,
	.freq				= 60,

	.output				= EXYNOS4412_FB_OUTPUT_RGB,
	.rgb_mode			= EXYNOS4412_FB_MODE_BGR_P,
	.bpp_mode			= EXYNOS4412_FB_BPP_MODE_32BPP,
	.swap				= EXYNOS4412_FB_SWAP_WORD,

	.rgba = {
		.r_mask			= 8,
		.r_field		= 0,
		.g_mask			= 8,
		.g_field		= 8,
		.b_mask			= 8,
		.b_field		= 16,
		.a_mask			= 8,
		.a_field		= 24,
	},

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

	.vram_front			= &vram[0][0],
	.vram_back			= &vram[1][0],

	.init				= lcd_init,
	.backlight			= lcd_backlight,
};

static struct exynos4412_fb_data_t vga_1440_900 = {
	.regbase			= EXYNOS4412_LCD_BASE,

	.width				= 1440,
	.height				= 900,
	.bits_per_pixel		= 32,
	.bytes_per_pixel	= 4,
	.freq				= 60,

	.output				= EXYNOS4412_FB_OUTPUT_RGB,
	.rgb_mode			= EXYNOS4412_FB_MODE_BGR_P,
	.bpp_mode			= EXYNOS4412_FB_BPP_MODE_32BPP,
	.swap				= EXYNOS4412_FB_SWAP_WORD,

	.rgba = {
		.r_mask			= 8,
		.r_field		= 0,
		.g_mask			= 8,
		.g_field		= 8,
		.b_mask			= 8,
		.b_field		= 16,
		.a_mask			= 8,
		.a_field		= 24,
	},

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

	.vram_front			= &vram[0][0],
	.vram_back			= &vram[1][0],

	.init				= lcd_init,
	.backlight			= lcd_backlight,
};

static struct exynos4412_fb_data_t vga_1280_1024 = {
	.regbase			= EXYNOS4412_LCD_BASE,

	.width				= 1280,
	.height				= 1024,
	.bits_per_pixel		= 32,
	.bytes_per_pixel	= 4,
	.freq				= 60,

	.output				= EXYNOS4412_FB_OUTPUT_RGB,
	.rgb_mode			= EXYNOS4412_FB_MODE_BGR_P,
	.bpp_mode			= EXYNOS4412_FB_BPP_MODE_32BPP,
	.swap				= EXYNOS4412_FB_SWAP_WORD,

	.rgba = {
		.r_mask			= 8,
		.r_field		= 0,
		.g_mask			= 8,
		.g_field		= 8,
		.b_mask			= 8,
		.b_field		= 16,
		.a_mask			= 8,
		.a_field		= 24,
	},

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

	.vram_front			= &vram[0][0],
	.vram_back			= &vram[1][0],

	.init				= lcd_init,
	.backlight			= lcd_backlight,
};

static struct fb_t exynos4412_fb;

static const struct gimage default_logo = {
  88, 100, 4,
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\337\347\363\0\0\0\0\0\322"
  "\337\355\3\315\333\354l\332\345\361\342\326\342\357\222\0\31\206\2\0\0\0"
  "\0\0\0\0\0\217\260\326\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\220\262\326\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\377"
  "\377\377\0\220\261\325\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\335\347\361\0\0\0\0\0\326\341\360\3\344\354\363l\373\373\375\377"
  "\376\366\366\377\373\372\373\376\341\352\363\250\0""5\230\3\0\0\0\0\0\0\0"
  "\0\214\257\325\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\255\305\340\10\333"
  "\345\361\22\366\367\373\21\363\365\372\21\363\365\372\21\363\365\372\21\363"
  "\365\372\21\363\365\372\21\363\365\372\21\363\365\372\21\363\365\372\21\363"
  "\365\372\21\363\365\372\21\363\365\372\21\364\366\372\21\347\356\365\21\260"
  "\307\341\13\0\0\0\0\377\377\377\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\335\347\361\0\0\0\0\0\301\324\347\3\344\354\365i\373\374\374\377"
  "\370\335\336\376\336OZ\376\364\306\311\377\372\375\377\375\340\351\364\240"
  "\\\217\303\6\0\0\0\0\0\0\0\0\213\256\324\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\231\270"
  "\331\17\305\326\351x\350\356\366\317\375\375\376\306\373\373\375\306\373"
  "\373\375\306\373\373\375\306\373\373\375\306\373\373\375\306\373\373\375"
  "\306\373\373\375\306\373\373\375\306\373\373\375\306\373\373\374\306\373"
  "\372\374\306\373\373\374\306\361\364\370\306\311\331\352\220\221\264\327"
  "\13\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\332\345\361\0\0\0\0\0"
  "\251\302\336\3\345\354\364h\372\375\376\377\367\335\337\376\3267C\376\314"
  "\0\15\377\325#0\376\365\312\315\377\371\375\376\374\337\352\364\230\216\257"
  "\323\11\0\0\0\0\377\377\377\0\214\256\324\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\313\332\353D\344"
  "\354\364\377\357\363\370\377\322\336\356\377\325\341\357\377\325\341\360"
  "\377\325\341\360\377\325\342\360\377\325\342\361\377\325\343\361\377\324"
  "\343\361\377\324\344\362\377\324\346\363\377\324\347\365\377\324\351\366"
  "\377\324\353\367\377\344\363\372\377\370\371\373\362\276\322\347i\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\331\345\360\0\0\0\0\0d\230\305\2\346\355"
  "\365g\373\377\377\377\366\334\336\377\324=H\376\310\1\20\377\313\12\30\377"
  "\314\5\24\377\324)6\376\365\311\314\377\370\375\377\374\337\352\364\220\235"
  "\271\333\13\0\0\0\0\377\377\377\0\213\256\324\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\325\342\357M\354\362"
  "\367\377\230\263\327\376\16N\245\377\34[\254\377\34]\255\377\34_\256\377"
  "\33a\261\377\33c\262\377\32e\263\377\32g\265\377\32k\270\377\31r\276\377"
  "\30{\305\377\27\204\314\377\31\217\323\377j\272\346\377\377\376\374\365\317"
  "\335\355\177\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\330\344\360\0\0\0\0\0\0@\231"
  "\1\347\356\365f\373\377\377\373\365\333\335\377\321BL\377\304\5\22\377\307"
  "\14\31\377\310\13\31\377\312\12\30\377\312\5\23\377\3240=\376\365\311\314"
  "\377\367\375\377\372\337\352\364\211\250\277\334\13\0\0\0\0\377\377\377\0"
  "\215\257\324\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\325\341\357L\354\362\370\377\216\251\322\377\0""9\230\377"
  "\5G\240\377\5I\242\377\4K\244\377\4N\246\377\4P\247\377\3R\251\377\3U\253"
  "\377\3W\254\377\2Z\260\377\2a\264\377\0i\273\377\3u\304\377[\253\335\377"
  "\377\377\375\364\316\334\354|\0\0\0\0\0\0\0\0\0\0\0\0\330\343\361\0\0\0\0"
  "\0\0\0\0\0\350\356\365e\373\377\377\366\364\332\334\377\317GQ\377\300\7\24"
  "\376\302\15\32\377\303\15\32\377\304\14\31\377\306\14\31\377\310\12\30\377"
  "\311\4\22\377\3256B\376\365\312\314\377\367\375\377\371\337\352\364\204\250"
  "\300\336\13\0\0\0\0\372\373\374\0\215\257\325\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\325\341\357L\354\362\370\377\220\251"
  "\322\377\0""8\227\377\11F\237\377\11H\241\377\11J\242\377\10L\244\377\10"
  "O\246\377\7Q\250\377\10T\251\377\7V\253\377\7X\255\377\7Z\257\377\6_\262"
  "\377\10i\272\377_\243\326\377\377\377\376\364\316\334\354|\0\0\0\0\0\0\0"
  "\0\331\344\361\0\0\0\0\0\0\0\0\0\351\356\365e\373\377\377\361\364\331\333"
  "\377\314LU\377\273\13\27\376\276\17\33\377\277\17\33\377\300\16\33\377\301"
  "\16\32\377\303\15\32\377\305\14\31\377\307\12\30\377\310\4\23\377\326;F\376"
  "\365\314\315\377\366\376\377\367\336\351\364\201\244\275\333\11\0\0\0\0\344"
  "\355\365\0\221\263\326\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\325\341\357L\354\362\370\377\220\247\321\377\0""4\225\377\12B\234"
  "\377\11D\236\377\11F\237\377\11I\242\377\10K\243\377\10N\245\377\7P\247\377"
  "\10R\250\377\7U\253\377\7W\254\377\6Y\255\377\12_\262\377a\232\317\377\377"
  "\377\377\364\316\334\354|\0\0\0\0\327\343\360\0\0\0\0\0\0\0\0\0\352\357\365"
  "d\374\377\377\354\363\331\333\377\313PX\377\270\15\32\376\271\21\34\377\273"
  "\21\34\377\273\20\34\377\275\17\33\377\276\17\33\377\300\16\32\377\302\15"
  "\32\377\304\15\31\377\306\13\30\377\307\4\22\377\326>I\376\366\316\320\377"
  "\367\376\377\366\334\350\363~\234\274\332\6\0\0\0\0\331\343\360\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\325\341\357L\354\362\370"
  "\377\220\246\317\377\0""0\222\377\12?\232\377\11A\234\377\11C\235\377\11"
  "F\237\377\10G\240\377\11J\243\377\10L\244\377\7O\246\377\10Q\250\377\10T"
  "\251\377\7V\253\377\12Z\256\377a\226\313\377\377\377\377\364\316\334\354"
  "|\0\0\0\0\0\0\0\0\0\0\0\0\352\357\365c\374\377\377\352\362\333\334\377\311"
  "T\\\377\264\21\33\376\265\21\35\377\266\22\35\377\267\22\35\377\270\21\34"
  "\377\272\21\34\377\273\20\34\377\275\17\33\377\277\16\33\377\301\15\32\377"
  "\304\15\31\377\306\13\30\377\310\4\23\377\326?K\376\366\321\324\377\367\376"
  "\377\366\332\346\361~\252\275\333\2\0\0\0\0\305\327\351\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\325\341\357L\354\362\370\377\220\245\316"
  "\377\0-\217\377\12<\230\377\12>\232\377\12@\233\377\11C\235\377\11E\237\377"
  "\10G\240\377\11J\242\377\10L\244\377\7N\245\377\7Q\247\377\7S\251\377\13"
  "X\254\377a\224\313\377\377\377\377\364\316\334\354|\0\0\0\0\0\0\0\0\347\354"
  "\364b\374\377\377\350\362\334\335\377\307X_\377\261\23\36\376\261\23\35\377"
  "\262\24\36\377\263\23\36\377\264\23\35\377\265\22\35\377\267\22\35\377\271"
  "\21\34\377\273\20\34\377\275\17\33\377\300\16\33\377\302\15\32\377\304\14"
  "\31\377\307\12\30\377\311\4\23\377\327?J\376\370\326\330\377\370\375\377"
  "\366\314\334\354|\325\341\360\7\0\0\0\0\30]\252\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\325\341\357L\354\362\370\377\221\243\315\377\0*\215\377"
  "\12""9\225\377\13;\227\377\12=\231\377\12@\233\377\11B\234\377\11D\236\377"
  "\10G\240\377\10H\241\377\10K\243\377\10N\245\377\7P\247\377\13T\252\377a"
  "\222\311\377\377\377\377\364\316\334\354|\0\0\0\0\271\315\345<\353\365\374"
  "\324\361\330\331\377\306X_\377\257\25\37\376\256\24\36\377\256\25\37\377"
  "\257\25\37\377\260\25\37\377\261\24\36\377\263\23\36\377\265\23\35\377\266"
  "\22\35\377\271\21\34\377\273\20\34\377\275\17\33\377\300\16\33\377\302\15"
  "\32\377\306\14\31\377\310\12\27\377\312\6\24\377\3279D\376\367\324\326\377"
  "\363\367\372\374\215\257\3251\0\0\7\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\325\341\357L\354\363\370\377\221\241\314\377\0&\212\377\13""5"
  "\222\377\13""8\224\377\12:\226\377\12<\230\377\12>\232\377\12A\233\377\11"
  "C\235\377\11E\237\377\10G\240\377\11J\242\377\10L\244\377\13Q\247\377b\220"
  "\307\377\377\377\377\364\316\334\354|\0\0\0\0\275\321\346A\356\366\374\333"
  "\370\347\350\377\276JR\376\251\20\32\377\253\27\40\377\253\27\40\377\254"
  "\26\40\377\254\25\37\377\256\25\37\377\257\25\37\377\261\24\36\377\263\23"
  "\36\377\266\22\35\377\270\21\34\377\273\20\34\377\276\17\33\377\300\16\32"
  "\377\304\15\32\377\306\14\31\377\306\0\15\377\3244@\376\370\331\332\377\356"
  "\365\372\377\272\317\345'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\325\341\357L\354\363\370\377\221\241\313\377\0#\210\377\14""1\217"
  "\377\13""3\221\377\13""6\223\377\12""9\225\377\13;\227\377\12=\231\377\12"
  "@\233\377\11B\234\377\11D\236\377\10F\240\377\10H\241\377\14M\245\377b\216"
  "\305\377\377\377\377\364\316\334\354|\0\0\0\0\0?\230\3\323\341\357\211\363"
  "\370\373\377\361\330\330\377\267AH\376\247\25\36\377\250\30!\377\250\30!"
  "\377\252\27\40\377\253\26\40\377\254\26\37\377\256\25\37\377\260\24\36\377"
  "\263\23\36\377\266\22\35\377\271\21\34\377\273\20\34\377\277\17\33\377\301"
  "\16\32\377\302\3\20\377\3201<\376\362\302\305\377\371\372\374\377\337\351"
  "\363\177\306\327\352\11\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0|\244\317\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0[\215\302\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\325\341\357L\354\363\370\377\221\241\313\377\0#\210\377\13/\216"
  "\377\14""0\217\377\13""2\220\377\13""5\222\377\13""8\224\377\12:\226\377"
  "\12<\230\377\12>\232\377\11A\233\377\11C\235\377\11E\237\377\14J\242\377"
  "c\213\303\377\377\377\377\364\316\334\354|\0\0\0\0\0\0\0\0,k\262\12\320\337"
  "\355\231\365\372\375\376\360\330\331\377\261;B\376\243\27\40\377\245\31!"
  "\377\246\31!\377\250\30\40\377\252\27\40\377\254\26\37\377\256\25\37\377"
  "\260\24\36\377\264\23\35\377\267\22\35\377\271\21\34\377\274\17\34\377\276"
  "\6\23\377\313/:\376\360\276\302\377\372\375\376\377\344\355\365\205\305\326"
  "\350\17\0\0\0\0\0\0\0\0{\243\316\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@{\271\14\217\261\325\21"
  "\213\256\324\20\213\256\324\20\213\256\324\20\213\256\324\20\213\256\324"
  "\20\213\256\324\20\213\256\324\20\213\256\324\20\213\256\324\20\213\256\324"
  "\20\213\256\324\20\213\256\324\20\213\256\324\20\213\256\324\21\0K\240\12"
  "\0\0\0\0\242\276\334\0\0\0\0\0\0\0\0\0\325\341\357L\354\363\370\377\221\241"
  "\313\377\0#\210\377\14/\216\377\13""0\217\377\14/\216\377\14""1\217\377\14"
  """3\220\377\13""5\223\377\12""8\225\377\13;\227\377\12=\231\377\11@\233\377"
  "\11A\234\377\15G\236\377b\211\302\377\377\377\377\364\316\334\354|\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0""7\225\10\321\337\356\242\367\374\376\376\356\327\330"
  "\377\2546=\376\241\31\"\377\243\32\"\377\245\31!\377\247\30!\377\251\27\40"
  "\377\253\26\37\377\256\25\37\377\261\24\36\377\265\22\35\377\270\21\35\377"
  "\271\12\27\377\306-8\376\355\272\275\377\373\376\377\377\345\355\365\214"
  "\277\323\347\20\0\0\0\0\0\0\0\0\265\313\344\0\0\0\0\0I\201\274\21\215\257"
  "\324\20\213\256\324\20\213\256\324\20\213\256\324\20\213\256\324\20\213\256"
  "\324\20\213\256\324\20\213\256\324\20\213\256\324\20\213\256\324\20\213\256"
  "\324\20\213\256\324\20\213\256\324\20\213\256\324\20\215\256\324\21\0,\216"
  "\6\0\0\0\0\267\314\344\1\277\322\347y\357\364\370\373\363\366\372\376\363"
  "\366\372\375\363\366\372\375\363\366\372\375\363\366\372\375\363\366\372"
  "\375\363\366\372\375\363\366\372\375\363\366\372\375\363\366\372\375\363"
  "\366\372\375\363\366\372\375\363\366\372\375\363\366\372\375\363\366\372"
  "\375\344\354\364\357\230\267\330P\0\0\0\0\0\0\0\0\0\0\0\0\325\341\357L\354"
  "\363\370\377\221\241\313\377\0#\210\377\14""0\217\377\14/\216\377\14/\216"
  "\377\13/\217\377\14""0\216\377\13""2\220\377\14""5\222\377\13""6\224\377"
  "\13:\226\377\12<\230\377\12>\232\377\15C\234\377c\206\300\377\377\377\377"
  "\364\316\334\354|\0\0\0\0g\226\307\0\0\0\0\0\0\0\0\0\0\0s\6\323\340\356\253"
  "\371\375\377\374\355\326\327\377\25039\376\237\33\"\377\241\33#\377\244\31"
  "!\377\247\30!\377\252\27\40\377\254\25\37\377\260\24\37\377\263\23\36\377"
  "\265\16\31\377\302,6\376\352\264\267\377\374\377\377\377\347\356\365\224"
  "\271\315\344\21\0\0\0\0\0\0\0\0A{\271\0\0\0\0\0\262\311\343\235\353\361\366"
  "\376\363\366\372\375\363\366\372\375\363\366\372\375\363\366\372\375\363"
  "\366\372\375\363\366\372\375\363\366\372\375\363\366\372\375\363\366\372"
  "\375\363\366\372\375\363\366\372\375\363\366\372\375\363\366\372\375\363"
  "\366\372\375\363\366\372\376\341\352\363\361\214\256\324;\341\352\363\3\343"
  "\354\364\377\366\370\373\377\313\331\353\376\316\333\354\377\316\333\354"
  "\377\316\333\354\377\316\333\354\377\316\333\354\377\316\333\354\377\316"
  "\333\354\377\316\333\354\377\316\333\354\377\316\333\354\377\316\334\354"
  "\377\316\334\354\377\316\334\355\377\317\335\355\376\377\376\376\375\354"
  "\361\367\271\0\0\0\0\0\0\0\0\0\0\0\0\325\341\357L\354\363\370\377\221\241"
  "\313\377\0#\210\377\13/\216\377\13/\216\377\13/\216\377\13/\216\377\13/\216"
  "\377\13/\216\377\13""1\217\377\13""3\220\377\12""5\223\377\12""8\224\377"
  "\12:\226\377\16?\232\377c\205\276\377\377\377\377\364\316\334\354|\0\0\0"
  "\0\0\0\0\0\0\0\0\0p\234\313\0\0\0\0\0\0\0V\6\325\341\356\262\373\377\377"
  "\373\353\323\324\377\24507\377\237\34#\376\242\33\"\377\246\31!\377\250\30"
  "\40\377\253\26\40\377\256\25\37\377\261\21\34\377\275,5\376\346\255\261\377"
  "\375\377\377\377\350\356\366\235\263\311\342\23\0\0\0\0\315\334\354\0\0\0"
  "\0\0\0\0\0\0\0D\233\31\372\373\375\372\361\367\373\376\314\343\363\377\315"
  "\344\363\377\314\345\363\377\315\345\363\377\315\345\364\377\315\345\364"
  "\377\314\346\364\377\314\346\365\377\314\346\365\377\314\346\365\377\314"
  "\347\365\377\314\347\365\377\314\347\365\377\313\347\365\377\323\353\367"
  "\376\375\376\376\377\311\332\352\267\347\356\366\3\347\356\366\377\317\333"
  "\353\376\0""4\226\377\0@\234\377\0@\235\377\0A\235\377\0A\236\377\0A\236"
  "\377\0B\236\377\0C\237\377\0C\237\377\0C\237\377\0D\240\377\0D\240\377\0"
  "D\240\377\0E\240\377\7M\245\377\377\377\377\376\330\344\360\340\0\0\0\0\0"
  "\0\0\0\0\0\0\0\324\341\357L\354\362\370\377\225\244\315\376\7,\215\377\25"
  """8\223\377\25""8\223\377\25""8\223\377\25""8\223\377\25""8\223\377\25""8"
  "\223\377\25""8\223\377\25""8\223\377\25:\225\377\25=\227\377\24>\231\377"
  "\27D\233\377i\207\277\377\377\377\377\364\316\334\354}\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0u\237\314\0\0\0\0\0\0\0O\6\327\342\357\270\373\377\377\374"
  "\353\320\323\377\246/6\377\240\33\"\377\244\31\"\377\247\30!\377\252\27\40"
  "\377\255\24\36\377\270,5\376\343\247\253\377\377\377\377\377\352\360\366"
  "\244\257\310\337\25\0\0\0\0\316\334\354\0\0\0\0\0\0\0\0\0\0\0\0\0*l\2604"
  "\376\376\375\371\270\330\356\377\0n\301\377\0r\305\377\0t\306\377\0v\307"
  "\377\0w\310\377\0x\311\377\0z\312\377\0|\314\377\0}\315\377\0\177\316\377"
  "\0\201\317\377\0\202\321\377\0\204\322\377\0\204\322\377\36\231\332\377\364"
  "\372\374\377\315\333\354\301\347\356\365\3\347\356\366\377\321\334\355\377"
  "\0<\233\377\11G\241\377\11G\241\377\11H\242\377\11I\241\377\11H\242\377\11"
  "I\243\377\11I\243\377\11J\242\377\11K\243\377\11K\244\377\11K\243\377\11"
  "L\244\377\10L\244\377\23T\250\377\377\377\377\376\331\344\360\335\0\0\0\0"
  "\0\0\0\0\0\0\0\0\314\333\353H\344\354\364\377\334\341\356\377\251\266\327"
  "\377\256\272\331\377\256\272\331\377\256\272\331\377\256\272\331\377\256"
  "\272\331\377\256\272\331\377\256\272\331\377\256\272\331\377\256\272\331"
  "\377\256\272\332\377\256\273\332\377\256\275\333\377\313\325\350\377\370"
  "\372\374\362\305\326\351r\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0y\242\315"
  "\0\0\0\0\0\0\0_\10\321\336\355\277\373\377\377\375\351\316\317\377\25007"
  "\376\242\30\40\377\246\31!\377\251\26\37\377\264-5\376\337\243\247\377\377"
  "\377\377\377\355\361\367\251\256\306\340\27\0\0\0\0\320\336\355\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0)k\2603\376\375\375\371\273\331\356\377\0r\302\377"
  "\2v\304\377\3x\306\377\2y\310\377\2{\311\377\2|\312\377\1~\313\377\2\177"
  "\314\377\2\201\315\377\1\202\316\377\0\205\320\377\0\205\321\377\0\207\322"
  "\377\0\207\323\377&\234\333\377\365\372\374\377\315\333\354\300\347\356\366"
  "\3\347\356\366\377\321\334\355\377\0<\232\377\10F\240\377\11G\241\377\11"
  "G\240\377\11H\241\377\11H\241\377\11H\242\377\11I\242\377\11I\242\377\11"
  "J\242\377\11J\243\377\11J\244\377\11K\243\377\10K\244\377\23S\250\377\377"
  "\377\377\376\331\344\360\335\0\0\0\0\0\0\0\0\0\0\0\0\276\323\346\34\342\353"
  "\363\237\377\377\377\345\377\377\377\336\377\377\377\336\377\377\377\336"
  "\377\377\377\336\377\377\377\336\377\377\377\336\377\377\377\336\377\377"
  "\377\336\377\377\377\336\377\377\377\336\377\377\377\336\377\377\377\336"
  "\377\377\377\336\377\377\377\341\355\362\370\263\266\314\343\33\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0p\234\312\0\0\0\0\0\0\0u\13\323\340"
  "\355\277\372\377\377\377\350\312\314\377\25317\376\244\25\36\377\261-5\376"
  "\334\240\243\377\377\377\377\377\356\363\367\255\260\310\341\30\0\0\0\0\322"
  "\340\356\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0)k\2603\376\375\375\371"
  "\274\330\355\377\0o\301\377\3t\303\377\2v\305\377\3x\306\377\3y\307\377\2"
  "{\310\377\2|\312\377\1~\313\377\1\177\314\377\2\201\315\377\1\202\316\377"
  "\1\205\320\377\0\205\321\377\0\205\321\377'\232\332\377\365\372\374\377\315"
  "\333\354\300\347\356\366\3\347\356\366\377\321\334\355\377\0;\232\377\11"
  "F\240\377\10F\240\377\10G\240\377\11G\240\377\11G\241\377\11H\241\377\11"
  "H\241\377\11I\242\377\11I\242\377\11J\242\377\11J\243\377\11J\243\377\10"
  "K\243\377\23S\250\377\377\377\377\376\331\344\360\335\0\0\0\0\0\0\0\0\323"
  "\341\356\0\0\0\0\0\0L\241\6\260\310\341(\377\377\377%\376\376\376%\376\376"
  "\376%\376\376\376%\376\376\376%\376\376\376%\376\376\376%\376\376\376%\376"
  "\376\376%\376\376\376%\376\376\376%\376\376\376%\377\376\376%\337\351\363"
  "#\25]\250\17\0\0\0\0\315\334\354\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0m\231\311\0\0\0\0\0\0/\223\17\323\340\356\276\371\376\377\377\350"
  "\305\307\377\265AH\376\331\233\237\377\377\377\377\377\357\363\370\260\263"
  "\311\341\33\0\0\0\0\325\342\357\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0)k\2603\376\375\375\371\274\330\355\377\0n\277\377\4r\302\377\3t"
  "\303\377\2v\304\377\2w\305\377\3y\307\377\2z\310\377\2{\311\377\1}\313\377"
  "\1\177\314\377\2\201\315\377\1\201\316\377\1\204\317\377\0\203\320\377'\230"
  "\330\377\365\372\374\377\315\333\354\300\347\356\365\3\347\356\366\377\321"
  "\334\355\377\0;\232\377\11E\237\377\11F\237\377\11F\240\377\10F\240\377\11"
  "G\240\377\11G\241\377\11H\241\377\11H\241\377\11H\242\377\11I\242\377\11"
  "I\242\377\11J\242\377\10J\243\377\23R\247\377\377\377\377\376\331\344\360"
  "\335\0\0\0\0\0\0\0\0T\207\277\0\377\377\377\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\223\264\327\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\224\264\326\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0""2\221"
  "\20\323\340\357\267\370\374\377\377\365\342\342\377\377\377\377\374\356\363"
  "\371\256\255\305\341\32\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\370\374\376"
  "\0\244\277\336\0\0\0\0\0\0\0\0\0)k\2603\376\375\375\371\273\327\355\377\0"
  "l\276\377\4p\300\377\3r\301\377\3s\302\377\3u\304\377\2v\305\377\3x\306\377"
  "\2z\307\377\2{\311\377\2|\313\377\1~\313\377\2\177\314\377\2\201\316\377"
  "\0\201\316\377'\227\327\377\365\372\374\377\315\333\354\300\347\356\366\3"
  "\347\356\366\377\321\334\355\377\0""9\231\377\12E\237\377\12E\237\377\11"
  "F\240\377\11F\240\377\10F\240\377\10G\240\377\11G\240\377\11H\241\377\11"
  "H\241\377\11H\242\377\11I\242\377\11I\243\377\10J\242\377\23R\247\377\377"
  "\377\377\376\331\344\360\335\0\0\0\0\0\0\0\0\377\377\377\0\0\0\0\0]\215\302"
  "\6\300\323\347\37\333\346\362\40\331\344\361\40\331\344\361\40\331\344\361"
  "\40\331\344\361\40\331\344\361\40\331\344\361\40\331\344\361\40\331\344\361"
  "\40\331\344\361\40\331\344\361\40\331\344\361\40\332\345\361\40\302\325\351"
  "\37F~\272\20\0\0\0\0\275\320\346\0\0\0\0\0\0\0\0\0\247\301\336\0\0\0\0\0"
  "(h\257\13\304\325\351\36\332\344\361\40\331\344\361\40\331\344\361\40\347"
  "\356\366\34\224\264\3309\332\345\362\310\350\362\371\371\343\353\365\312"
  "\313\333\354B\335\347\362\26\331\344\361!\331\344\361\40\335\347\362\40\265"
  "\313\344#c\222\307\10\0\0\0\0\374\375\375\0\0\0\0\0\0\0\0\0)k\2603\376\375"
  "\375\371\273\327\354\377\0i\274\377\3n\277\377\4o\300\377\4q\301\377\3s\302"
  "\377\3t\303\377\2v\305\377\3w\306\377\3y\307\377\2z\310\377\2|\312\377\1"
  "}\313\377\1\177\314\377\0\177\314\377'\225\325\377\365\371\374\377\315\333"
  "\354\300\347\356\366\3\347\356\366\377\321\334\354\377\0""9\231\377\12D\236"
  "\377\11E\237\377\12E\237\377\11E\240\377\11F\237\377\11F\240\377\10G\240"
  "\377\11G\240\377\11G\241\377\11H\241\377\11H\241\377\11H\242\377\10I\242"
  "\377\23P\246\377\377\377\377\376\331\344\360\335\0\0\0\0\0\0\0\0\0\0\0\0"
  "\251\302\336\23\323\337\356v\366\370\373\266\377\377\377\264\377\377\376"
  "\264\377\377\376\264\377\377\376\264\377\377\376\264\377\377\376\264\377"
  "\377\376\264\377\377\376\264\377\377\375\264\377\376\375\264\377\376\375"
  "\264\377\376\375\264\377\375\374\264\366\367\372\265\323\341\356\212\243"
  "\274\333\23\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0Z\213\303\15\322\340\356\177\366"
  "\370\372\263\377\377\376\264\377\377\376\264\377\377\376\264\377\377\376"
  "\264\377\377\377\260\365\370\371\271\344\354\363\364\354\361\366\314\377"
  "\377\377\253\377\376\375\264\377\376\375\264\377\375\374\264\377\376\375"
  "\264\362\365\370\272\332\345\361\177\310\330\352\26\0\0\0\0\0\0\0\0\0\0\0"
  "\0)k\2603\376\375\375\371\274\326\354\377\0g\272\377\4l\275\377\3m\276\377"
  "\3o\277\377\4q\300\377\3r\302\377\3s\303\377\3u\304\377\2w\305\377\3x\306"
  "\377\2z\310\377\2{\311\377\2}\312\377\0}\313\377(\223\324\377\365\371\374"
  "\377\315\333\354\300\347\356\366\3\347\356\366\377\321\334\354\377\0""8\230"
  "\377\12D\236\377\12D\236\377\11D\237\377\12E\237\377\12E\237\377\11F\237"
  "\377\11F\240\377\10F\240\377\11G\241\377\11G\241\377\11H\241\377\11H\241"
  "\377\10H\242\377\23P\246\377\377\377\377\376\331\344\360\335\0\0\0\0\0\0"
  "\0\0\0\0\0\0\307\327\352A\342\353\363\377\347\355\365\377\301\322\347\377"
  "\304\325\351\377\304\326\351\377\304\326\352\377\304\327\352\377\304\330"
  "\353\377\304\330\353\377\304\330\354\377\304\332\355\377\304\334\357\377"
  "\304\337\361\377\304\341\362\377\304\343\364\377\332\356\370\377\367\370"
  "\373\362\275\321\346b\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\317\335\355@\371\373"
  "\374\345\337\350\362\377\307\326\351\377\304\325\351\377\304\326\352\377"
  "\304\326\352\377\304\327\352\377\305\330\353\377\305\331\354\377\305\331"
  "\354\377\304\332\355\377\304\335\357\377\304\337\361\377\304\341\363\377"
  "\300\342\364\377\342\362\372\377\356\363\370\377\343\353\364D\0\0\0\0\0\0"
  "\0\0\0\0\0\0)k\2603\376\375\375\371\274\326\353\377\0d\270\377\5i\273\377"
  "\4k\275\377\4m\276\377\3n\277\377\4o\300\377\4q\301\377\3s\302\377\3t\303"
  "\377\2v\305\377\3w\306\377\3x\307\377\2z\311\377\0z\311\377(\221\323\377"
  "\365\371\374\377\315\333\354\300\347\356\366\3\347\356\366\377\321\333\354"
  "\377\0""8\227\377\12C\236\377\12D\236\377\12D\236\377\12D\237\377\11E\237"
  "\377\12E\237\377\11F\240\377\11F\240\377\10F\240\377\10G\240\377\11G\240"
  "\377\11G\241\377\10H\241\377\23O\245\377\377\377\377\376\331\344\360\335"
  "\0\0\0\0\0\0\0\0\0\0\0\0\324\340\357M\352\360\367\377\242\272\333\376$^\255"
  "\3770i\263\377/k\265\377/m\266\377/o\270\377/q\271\377/s\272\377/u\274\377"
  "/w\276\377.}\302\377-\204\310\377+\214\316\377.\225\325\377w\277\347\377"
  "\377\375\374\366\314\333\354\200\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\371\373"
  "\374W\377\377\377\346\217\253\323\3777l\264\3770i\263\377/k\265\377/m\266"
  "\377/n\267\377/q\271\377/s\272\377/u\274\377/x\276\377.}\302\377-\206\311"
  "\377,\216\317\377\37\221\324\377\216\312\353\376\364\370\373\377\366\370"
  "\372M\0\0\0\0\0\0\0\0\0\0\0\0)k\2603\376\375\375\371\274\325\353\377\0b\266"
  "\377\5g\271\377\5h\273\377\4j\274\377\4l\275\377\3m\277\377\3o\277\377\4"
  "p\301\377\3r\302\377\3s\303\377\2u\304\377\2w\306\377\3x\307\377\0x\307\377"
  "(\217\321\377\365\371\374\377\315\333\354\300\347\356\366\3\347\356\366\377"
  "\321\333\354\377\0""7\227\377\12C\235\377\12C\236\377\12C\236\377\12D\236"
  "\377\12D\236\377\11D\237\377\12E\237\377\11E\237\377\11F\237\377\11F\240"
  "\377\10G\240\377\11G\241\377\10G\241\377\23O\245\377\377\377\377\376\331"
  "\344\360\335\0\0\0\0\0\0\0\0\0\0\0\0\325\341\357L\355\362\370\377\216\251"
  "\323\377\0""8\231\377\4E\241\377\4H\242\377\3J\244\377\3M\246\377\3O\247"
  "\377\2Q\251\377\2T\253\377\2V\255\377\1Y\257\377\0_\264\377\0g\273\377\1"
  "t\303\377[\251\334\377\377\377\375\364\317\334\354|\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\366\371\373Y\377\377\377\346w\230\311\377\14I\242\377\3E\240\377"
  "\3H\242\377\3J\244\377\3L\245\377\3P\250\377\2Q\251\377\2T\252\377\2V\255"
  "\377\1Y\257\377\0`\264\377\0i\273\377\0m\301\377v\270\343\377\364\370\373"
  "\377\370\371\373L\0\0\0\0\0\0\0\0\0\0\0\0)k\2603\376\376\375\371\274\324"
  "\353\377\0`\266\377\4e\270\377\4f\270\377\5h\272\377\4j\274\377\4k\275\377"
  "\4m\276\377\3n\277\377\4p\300\377\3r\301\377\3s\303\377\3u\304\377\2w\305"
  "\377\0u\306\377)\215\317\377\365\371\374\377\315\333\354\300\347\356\366"
  "\3\347\356\366\377\321\334\354\377\0""6\226\377\12B\235\377\12B\235\377\12"
  "C\235\377\12C\236\377\12D\236\377\12D\236\377\11D\237\377\12E\237\377\12"
  "E\237\377\11F\237\377\11F\240\377\10F\240\377\10F\240\377\23O\244\377\377"
  "\377\377\376\331\344\360\335\0\0\0\0\0\0\0\0\0\0\0\0\325\341\357L\354\362"
  "\370\377\220\251\321\377\0""8\227\377\11F\240\377\10H\242\377\10J\243\377"
  "\10M\245\377\10O\247\377\10Q\250\377\7S\252\377\7V\253\377\6X\255\377\6["
  "\260\377\6^\263\377\10h\271\377_\241\325\377\377\377\376\364\316\334\354"
  "|\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\366\371\373X\377\377\377\346z\230\311\377"
  "\21I\240\377\11E\237\377\10H\241\377\11J\243\377\10L\244\377\7O\247\377\7"
  "Q\250\377\7S\252\377\7V\253\377\6X\255\377\6[\257\377\6_\263\377\0_\266\377"
  "z\262\334\377\364\370\373\377\370\371\373L\0\0\0\0\0\0\0\0\0\0\0\0)k\260"
  "3\376\376\375\371\274\324\352\377\0^\264\377\5c\266\377\5d\267\377\4f\270"
  "\377\5g\271\377\5i\273\377\4k\275\377\4l\276\377\3n\277\377\4o\300\377\4"
  "q\301\377\3r\302\377\3t\304\377\0t\304\377)\213\316\377\365\371\374\377\315"
  "\333\354\300\347\356\366\3\347\356\366\377\321\334\354\377\0""6\226\377\12"
  "A\234\377\12B\235\377\12B\235\377\12C\235\377\12C\236\377\12C\236\377\12"
  "D\236\377\12D\236\377\11E\237\377\12E\237\377\11F\240\377\11F\237\377\10"
  "F\240\377\23N\244\377\377\377\377\376\331\344\360\335\0\0\0\0\0\0\0\0\0\0"
  "\0\0\325\341\357L\354\362\370\377\220\247\321\377\0""4\224\377\11B\234\377"
  "\11D\236\377\11F\240\377\10I\242\377\10K\244\377\10N\246\377\7P\247\377\10"
  "R\251\377\7T\252\377\6W\254\377\6Y\256\377\12^\261\377`\232\316\377\377\377"
  "\377\364\316\334\354|\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\366\371\373X\377\377"
  "\377\346{\226\307\377\21F\236\377\11A\234\377\11D\236\377\10F\240\377\11"
  "I\242\377\10K\243\377\10M\245\377\7P\247\377\10S\251\377\7T\252\377\6W\254"
  "\377\6Y\256\377\0T\255\377{\252\326\377\364\370\373\377\370\372\373L\0\0"
  "\0\0\0\0\0\0\0\0\0\0)k\2603\376\376\375\371\274\323\351\377\0\\\261\377\6"
  "`\264\377\6b\266\377\5c\267\377\4e\270\377\5g\271\377\5h\272\377\4j\273\377"
  "\4l\275\377\3m\276\377\4o\277\377\4p\301\377\3r\302\377\0q\302\377(\211\315"
  "\377\365\371\374\377\315\333\354\300\347\356\366\3\347\356\366\377\321\334"
  "\354\377\0""6\226\377\12@\233\377\12A\234\377\12B\235\377\12B\234\377\12"
  "C\235\377\12C\236\377\12C\235\377\12D\236\377\12D\236\377\11E\237\377\12"
  "E\237\377\12E\237\377\11E\237\377\22N\243\377\377\377\377\376\331\344\360"
  "\335\0\0\0\0\0\0\0\0\0\0\0\0\325\341\357L\354\362\370\377\220\246\320\377"
  "\0""1\223\377\11@\233\377\12B\234\377\11D\236\377\11F\240\377\10H\242\377"
  "\10K\244\377\10N\246\377\7P\247\377\10R\251\377\7T\253\377\6V\254\377\12"
  "\\\257\377a\226\314\377\377\377\377\364\316\334\354|\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\366\371\373X\377\377\377\346{\225\306\377\22C\234\377\11?\232"
  "\377\11B\234\377\11D\236\377\10F\237\377\10I\242\377\11K\243\377\10M\245"
  "\377\7P\246\377\7R\250\377\7T\253\377\7W\254\377\0P\252\377|\250\325\377"
  "\364\367\373\377\370\372\373L\0\0\0\0\0\0\0\0\0\0\0\0)k\2603\376\376\375"
  "\371\274\323\351\377\0Z\260\377\6_\263\377\6a\265\377\6c\266\377\5d\267\377"
  "\4e\270\377\4g\271\377\5h\272\377\5j\273\377\4l\275\377\3m\276\377\3n\277"
  "\377\4p\300\377\0p\301\377)\210\314\377\365\371\374\377\315\333\354\300\347"
  "\356\366\3\347\356\366\377\321\333\354\377\0""5\225\377\12A\234\377\12A\234"
  "\377\12A\234\377\12B\235\377\12B\235\377\12B\235\377\12C\235\377\12C\236"
  "\377\12D\235\377\12D\236\377\12D\237\377\11E\237\377\11E\237\377\23M\243"
  "\377\377\377\377\376\331\344\360\335\0\0\0\0\0\0\0\0\0\0\0\0\325\341\357"
  "L\354\362\370\377\220\245\317\377\0.\220\377\12<\230\377\12>\232\377\11@"
  "\233\377\12C\235\377\11E\237\377\10H\241\377\11J\243\377\10L\245\377\7O\246"
  "\377\7Q\250\377\7S\252\377\13X\254\377a\225\312\377\377\377\377\364\316\334"
  "\354|\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\366\371\373X\377\377\377\346{\222\305"
  "\377\22@\231\377\12;\227\377\12?\232\377\11A\234\377\11C\235\377\11E\237"
  "\377\10G\240\377\11J\242\377\10L\244\377\7N\246\377\7Q\250\377\10S\252\377"
  "\0M\247\377|\246\323\377\364\370\372\377\370\372\374L\0\0\0\0\0\0\0\0\0\0"
  "\0\0)k\2603\376\376\375\371\274\323\351\377\1Y\260\377\5^\262\377\6_\263"
  "\377\6`\264\377\5b\265\377\5c\267\377\5d\267\377\4g\271\377\5h\272\377\4"
  "i\273\377\4k\274\377\4l\275\377\3n\277\377\0n\277\377)\206\312\377\365\371"
  "\374\377\315\333\354\300\347\356\366\3\347\356\366\377\321\332\354\377\0"
  """4\225\377\12@\233\377\12@\233\377\12A\234\377\12@\233\377\12A\234\377\12"
  "B\235\377\12B\234\377\12B\235\377\12C\236\377\12D\236\377\12C\235\377\12"
  "D\236\377\11D\236\377\24M\243\377\377\377\377\376\331\344\360\335\0\0\0\0"
  "\0\0\0\0\0\0\0\0\325\341\357L\354\362\370\377\221\242\315\377\0*\215\377"
  "\12""8\226\377\13;\227\377\12=\231\377\11@\233\377\11A\234\377\12D\236\377"
  "\11F\240\377\11I\242\377\10K\244\377\10N\246\377\7P\247\377\13U\252\377b"
  "\222\311\377\377\377\377\364\316\334\354|\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\366\371\373X\377\377\377\346{\221\303\377\23<\227\377\12""8\224\377\12;"
  "\226\377\12=\231\377\11@\233\377\11B\234\377\11D\236\377\10F\237\377\10I"
  "\242\377\11K\243\377\10M\245\377\7P\246\377\0I\244\377|\244\322\377\364\367"
  "\373\377\370\372\374L\0\0\0\0\0\0\0\0\0\0\0\0)k\2603\376\376\375\371\274"
  "\323\351\377\2X\257\377\6]\261\377\5]\262\377\5^\263\377\6`\264\377\6a\265"
  "\377\6c\266\377\5d\267\377\4f\270\377\5g\271\377\5i\273\377\5j\274\377\4"
  "l\275\377\0k\276\377)\205\311\377\365\371\373\377\315\333\354\300\346\356"
  "\366\3\347\356\366\377\321\332\353\377\0""3\224\377\11?\233\377\11@\232\377"
  "\12@\233\377\12A\234\377\12@\233\377\12A\234\377\12A\235\377\12B\234\377"
  "\12C\235\377\12B\236\377\12C\235\377\12D\235\377\11C\236\377\24L\242\377"
  "\377\377\377\376\331\344\360\335\0\0\0\0\0\0\0\0\0\0\0\0\325\341\357L\354"
  "\363\370\377\221\242\314\377\0&\212\377\13""5\223\377\12""7\225\377\13""9"
  "\226\377\13<\230\377\12>\232\377\11@\233\377\12C\235\377\11E\237\377\11H"
  "\241\377\11J\243\377\10L\245\377\13Q\247\377b\220\307\377\377\377\377\364"
  "\316\334\354|\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\366\371\373X\377\377\377\346"
  "{\217\302\377\23""8\224\377\13""5\222\377\13""7\224\377\13""9\225\377\13"
  "<\230\377\12?\232\377\11A\234\377\12C\235\377\11E\237\377\10G\240\377\11"
  "J\242\377\10L\244\377\0E\241\377|\242\320\377\364\370\373\377\370\372\374"
  "L\0\0\0\0\0\0\0\0\0\0\0\0)k\2603\376\376\375\371\274\322\351\377\2X\257\377"
  "\7\\\260\377\7]\261\377\6]\262\377\5^\262\377\5_\263\377\5`\264\377\6b\266"
  "\377\5c\267\377\4f\270\377\5g\271\377\4h\272\377\5j\273\377\1j\274\377)\202"
  "\310\377\365\371\373\377\315\333\354\300\346\356\366\3\347\356\366\377\322"
  "\332\353\377\0""3\224\377\11?\232\377\12?\233\377\11@\232\377\12?\232\377"
  "\12@\233\377\12A\234\377\12@\233\377\12A\234\377\12A\234\377\12B\234\377"
  "\12B\235\377\12C\236\377\11C\235\377\24K\242\377\377\377\377\376\331\344"
  "\360\335\0\0\0\0\0\0\0\0\0\0\0\0\325\341\357L\354\363\370\377\221\241\313"
  "\377\0#\210\377\14""1\220\377\14""4\222\377\13""6\224\377\12""9\226\377\13"
  ";\227\377\12=\231\377\12@\233\377\12A\234\377\12D\236\377\11F\240\377\10"
  "H\241\377\14M\245\377b\215\305\377\377\377\377\364\316\334\354|\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\366\371\373X\377\377\377\346{\216\302\377\23""7\222"
  "\377\13""1\217\377\14""4\221\377\14""6\223\377\13""8\224\377\13:\226\377"
  "\12=\231\377\11@\233\377\12B\234\377\11D\236\377\10F\237\377\11I\242\377"
  "\0B\237\377}\240\317\377\364\367\372\377\370\372\374L\0\0\0\0\0\0\0\0\0\0"
  "\0\0)k\2603\376\376\375\371\274\323\351\377\2W\256\377\7[\261\377\6\\\260"
  "\377\7]\261\377\6\\\261\377\5]\262\377\6^\263\377\5`\264\377\6a\265\377\6"
  "c\266\377\5e\267\377\4f\270\377\4g\271\377\1g\272\377*\201\306\377\365\371"
  "\373\377\315\333\354\300\346\356\366\3\347\356\366\377\322\332\353\377\0"
  """2\224\377\13>\231\377\12>\231\377\11>\232\377\11?\233\377\11?\232\377\12"
  "@\233\377\12A\234\377\12@\233\377\12A\234\377\12A\235\377\12B\234\377\12"
  "B\235\377\11B\235\377\24K\241\377\377\377\377\376\331\344\360\335\0\0\0\0"
  "\0\0\0\0\0\0\0\0\325\341\357L\354\363\370\377\221\241\313\377\0#\210\377"
  "\14/\216\377\13""0\217\377\14""2\221\377\13""5\223\377\13""7\225\377\13:"
  "\226\377\13<\230\377\12>\232\377\11@\233\377\12C\235\377\10E\237\377\14J"
  "\242\377c\214\304\377\377\377\377\364\316\334\354|\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\366\371\373X\377\377\377\346{\216\302\377\23""6\222\377\13/\216"
  "\377\14""0\217\377\13""2\220\377\14""5\222\377\13""7\223\377\13""9\225\377"
  "\13<\230\377\12?\232\377\12A\233\377\12C\235\377\11E\237\377\0>\233\377|"
  "\237\315\377\364\367\373\377\370\372\374L\0\0\0\0\0\0\0\0\0\0\0\0)k\2603"
  "\376\376\375\371\274\322\351\377\2W\256\377\7[\260\377\7[\260\377\6[\260"
  "\377\7\\\260\377\7\\\261\377\6]\261\377\5^\262\377\5_\263\377\5a\264\377"
  "\6c\266\377\5d\267\377\5f\270\377\1e\270\377*\177\304\377\365\370\373\377"
  "\315\333\354\300\346\356\366\3\347\356\366\377\322\332\353\377\0""1\223\377"
  "\12=\231\377\13=\231\377\12>\231\377\12>\232\377\12?\232\377\11?\232\377"
  "\12@\233\377\12@\233\377\12@\233\377\12A\234\377\12A\234\377\12A\234\377"
  "\11B\234\377\24J\241\377\377\377\377\376\331\344\360\335\0\0\0\0\0\0\0\0"
  "\0\0\0\0\325\341\357L\354\363\370\377\221\241\313\377\0#\210\377\14""0\217"
  "\377\14/\216\377\13""0\217\377\14""1\221\377\14""3\222\377\13""6\224\377"
  "\12""9\225\377\13;\227\377\12=\230\377\12?\232\377\11A\234\377\15G\240\377"
  "c\211\302\377\377\377\377\364\316\334\354|\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\366\371\373X\377\377\377\346{\216\302\377\23""6\222\377\13/\216\377\14"
  "/\216\377\14/\216\377\14""1\217\377\14""4\221\377\13""5\222\377\13""8\224"
  "\377\13:\226\377\12<\230\377\12?\232\377\12B\234\377\0:\231\377}\234\313"
  "\377\364\367\372\377\370\372\374L\0\0\0\0\0\0\0\0\0\0\0\0)k\2603\376\376"
  "\375\371\274\322\350\377\2V\256\377\7Z\257\377\7Z\257\377\7[\260\377\7[\260"
  "\377\6\\\260\377\7\\\261\377\6]\261\377\6]\262\377\5^\263\377\5`\264\377"
  "\6b\266\377\6c\266\377\1c\266\377*}\303\377\365\370\373\377\315\333\354\300"
  "\346\356\366\3\347\356\366\377\322\332\353\377\0""1\222\377\13<\230\377\12"
  "=\231\377\13=\231\377\13>\232\377\12>\231\377\12>\232\377\11?\232\377\12"
  "?\232\377\12@\233\377\12@\233\377\12@\233\377\12A\234\377\11A\234\377\24"
  "J\240\377\377\377\377\376\331\344\360\335\0\0\0\0\0\0\0\0\0\0\0\0\325\341"
  "\357L\354\363\370\377\221\241\313\377\0#\210\377\14""0\217\377\14""0\217"
  "\377\14/\216\377\14/\216\377\13""1\220\377\14""2\221\377\13""4\223\377\13"
  """7\225\377\13:\226\377\13<\230\377\11>\231\377\15C\235\377c\207\300\377"
  "\377\377\377\364\316\334\354|\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\366\371\373"
  "X\377\377\377\346{\216\302\377\23""6\222\377\13/\216\377\14""0\217\377\14"
  """0\217\377\14/\216\377\13""0\216\377\14""2\220\377\13""4\221\377\13""7\223"
  "\377\13""9\225\377\13<\230\377\12>\232\377\0""6\226\377}\233\312\377\364"
  "\367\372\377\370\372\374L\0\0\0\0\0\0\0\0\0\0\0\0)k\2603\376\376\375\371"
  "\274\322\351\377\2V\255\377\7Z\257\377\7Z\257\377\7Z\260\377\7[\260\377\7"
  "[\260\377\6[\260\377\7\\\260\377\6\\\261\377\6]\262\377\6^\262\377\5_\263"
  "\377\5a\265\377\2a\265\377*{\302\377\365\370\373\377\315\333\354\300\346"
  "\356\366\3\347\356\366\377\322\332\353\377\0""0\222\377\13<\230\377\13<\230"
  "\377\13=\231\377\12=\231\377\13=\231\377\12>\231\377\12>\232\377\11?\232"
  "\377\11?\232\377\12@\233\377\12@\233\377\12@\233\377\11A\234\377\24I\241"
  "\377\377\377\377\376\331\344\360\335\0\0\0\0\0\0\0\0\0\0\0\0\325\341\357"
  "L\354\363\370\377\220\240\313\377\0!\207\377\11-\215\377\11-\215\377\11-"
  "\215\377\11-\215\377\11-\215\377\11.\216\377\11/\217\377\11""1\221\377\10"
  """4\223\377\10""6\224\377\10""8\226\377\13>\231\377b\204\276\377\377\377"
  "\377\364\316\334\354|\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\366\371\373Y\377\377"
  "\377\346z\215\301\377\21""4\221\377\11-\215\377\11-\215\377\11-\215\377\11"
  "-\215\377\11-\215\377\11-\215\377\11/\216\377\11""2\220\377\11""3\221\377"
  "\10""6\223\377\10""8\225\377\0""0\222\377|\227\311\377\364\367\372\377\370"
  "\372\374L\0\0\0\0\0\0\0\0\0\0\0\0)k\2603\376\376\375\371\274\322\350\377"
  "\2U\255\377\7Y\256\377\7Y\257\377\7Z\257\377\7Z\260\377\7Z\257\377\7[\260"
  "\377\6[\260\377\7\\\260\377\7\\\261\377\6]\261\377\6]\262\377\5^\263\377"
  "\2^\264\377+y\300\377\365\370\373\377\315\333\354\300\346\356\366\3\347\356"
  "\366\377\322\332\353\377\0/\221\377\13;\230\377\13<\230\377\13<\230\377\13"
  "=\230\377\12=\231\377\13=\231\377\12>\231\377\12>\232\377\12?\232\377\11"
  "?\232\377\12?\232\377\12@\233\377\11@\233\377\24H\237\377\377\377\377\376"
  "\331\344\360\335\0\0\0\0\0\0\0\0\0\0\0\0\324\341\357M\353\362\370\377\230"
  "\247\317\376\17""3\220\377\35>\226\377\35>\226\377\35>\226\377\35>\226\377"
  "\35>\226\377\35>\226\377\35>\226\377\35?\227\377\35A\231\377\34C\232\377"
  "\33E\234\377\37J\237\377n\213\302\377\377\377\377\365\316\334\354~\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\371\373\374X\377\377\377\346\204\226\306\377$"
  "D\232\377\34>\226\377\35>\226\377\35>\226\377\35>\226\377\35>\226\377\35"
  ">\226\377\35>\226\377\35?\226\377\35A\230\377\34C\231\377\34E\233\377\20"
  ">\230\377\206\236\313\376\365\370\372\377\370\372\374L\0\0\0\0\0\0\0\0\0"
  "\0\0\0)k\2603\376\376\375\371\274\321\350\377\2T\254\377\7X\255\377\7Y\256"
  "\377\7Y\257\377\7Z\257\377\7Z\257\377\7[\257\377\7[\260\377\7[\260\377\6"
  "\\\260\377\7\\\261\377\6\\\261\377\6]\262\377\2\\\262\377+w\276\377\365\370"
  "\373\377\315\333\354\300\346\356\366\3\347\356\366\377\322\332\353\377\0"
  "/\221\377\13;\227\377\13;\230\377\13<\230\377\13<\230\377\13=\230\377\12"
  "=\231\377\13=\231\377\13>\232\377\12>\231\377\12>\232\377\11?\233\377\12"
  "?\232\377\11@\233\377\24H\240\377\377\377\377\376\331\344\360\335\0\0\0\0"
  "\0\0\0\0\0\0\0\0\320\336\355F\347\356\366\377\326\333\353\377\233\252\320"
  "\377\241\257\323\377\241\257\323\377\241\257\323\377\241\257\323\377\241"
  "\257\323\377\241\257\323\377\241\257\323\377\241\256\323\377\241\257\323"
  "\377\241\260\324\377\240\261\325\377\242\263\326\377\303\317\345\377\373"
  "\375\375\364\310\330\352n\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\334\346\362M\377"
  "\377\377\350\314\324\347\377\244\261\324\377\241\256\323\377\241\257\323"
  "\377\241\257\323\377\241\257\323\377\241\257\323\377\241\257\323\377\241"
  "\257\323\377\241\257\323\377\241\257\323\377\241\260\324\377\241\261\324"
  "\377\233\256\323\377\316\327\351\377\357\363\370\377\347\357\365J\0\0\0\0"
  "\0\0\0\0\0\0\0\0)k\2603\376\376\375\371\274\322\350\377\2T\253\377\7X\256"
  "\377\7X\256\377\7Y\256\377\7Y\256\377\7Z\257\377\7Z\257\377\7Z\257\377\7"
  "[\260\377\7[\260\377\6[\261\377\7\\\260\377\6\\\261\377\3[\261\377+v\275"
  "\377\365\370\373\377\315\333\354\300\346\356\366\3\347\356\366\377\322\332"
  "\353\377\0/\220\377\13:\227\377\13;\227\377\13;\230\377\13<\227\377\13<\230"
  "\377\13<\230\377\13=\231\377\12=\231\377\13>\231\377\12>\231\377\12>\232"
  "\377\11?\233\377\11?\232\377\24H\237\377\377\377\377\376\331\344\360\335"
  "\0\0\0\0\0\0\0\0\0\0\0\0\303\325\351\33\334\347\362\226\374\376\376\324\377"
  "\377\377\315\377\377\377\315\377\377\377\315\377\377\377\315\377\377\377"
  "\315\377\377\377\315\377\377\377\315\377\377\377\315\377\377\377\315\377"
  "\377\377\315\377\377\377\315\377\377\377\315\377\377\377\315\377\377\377"
  "\317\345\354\365\243\273\317\346!\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\222\264"
  "\327\32\342\353\364\234\377\377\377\316\377\377\377\316\377\377\377\315\377"
  "\377\377\315\377\377\377\315\377\377\377\315\377\377\377\315\377\377\377"
  "\315\377\377\377\315\377\377\377\315\377\377\377\315\377\377\377\315\377"
  "\377\377\315\377\377\377\315\377\377\377\322\333\346\361\251\276\322\346"
  "%\0\0\0\0\0\0\0\0\0\0\0\0)k\2603\376\376\375\371\274\321\350\377\2T\253\377"
  "\7W\255\377\7X\256\377\7X\256\377\7Y\256\377\7Y\256\377\7Y\257\377\7Z\257"
  "\377\7Z\260\377\7[\260\377\7[\260\377\6[\260\377\7\\\260\377\3Z\260\377+"
  "u\275\377\365\370\373\377\315\333\354\300\346\356\366\3\347\356\366\377\322"
  "\332\353\377\0.\217\377\13:\226\377\13:\227\377\13;\227\377\13;\227\377\13"
  ";\230\377\13<\230\377\13<\230\377\13=\231\377\12=\231\377\13=\231\377\12"
  ">\232\377\12>\232\377\11?\232\377\24F\237\377\377\377\377\376\331\344\360"
  "\335\0\0\0\0\0\0\0\0\377\377\377\0\0\0\0\0\256\310\341\22\340\352\3639\377"
  "\377\3775\375\375\3755\375\375\3755\375\375\3755\375\375\3755\375\375\375"
  "5\375\375\3755\375\375\3755\375\375\3755\375\375\3755\375\375\3755\375\375"
  "\3755\376\376\3765\360\364\3714\256\306\341\36\0\0\0\0\320\335\355\0\0\0"
  "\0\0\0\0\0\0\327\343\360\0\0\0\0\0\240\276\334\33\357\363\3713\376\376\376"
  "5\376\376\3755\375\375\3755\375\375\3755\375\375\3755\375\375\3755\375\375"
  "\3755\375\375\3755\375\375\3755\375\375\3755\375\375\3755\375\375\3755\377"
  "\377\3765\354\361\3678\253\304\340\27\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0)k\260"
  "3\376\376\375\371\274\321\350\377\2S\252\377\7W\255\377\7W\255\377\7X\256"
  "\377\7X\255\377\7X\256\377\7Y\256\377\7Y\257\377\7Z\257\377\7Z\257\377\7"
  "[\257\377\7[\260\377\7[\260\377\3Z\257\377,t\275\377\365\370\373\377\315"
  "\333\354\300\346\356\366\3\347\356\366\377\322\332\353\377\0-\217\377\13"
  """9\226\377\13:\226\377\13:\227\377\13;\227\377\13;\227\377\13;\230\377\13"
  "<\230\377\13<\230\377\13=\230\377\12=\231\377\13=\231\377\13>\231\377\12"
  ">\231\377\23F\236\377\377\377\377\376\331\344\360\335\0\0\0\0\0\0\0\0\231"
  "\270\330\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\315\334\354\0\261\310\341\0\0\0\0\0\0\0\0\0@z\267\0\340\352\364"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\205\251\322\0\0\0\0\0\0\0\0\0)k\2603\376\376\375\371\274\321\350\377\1"
  "R\252\377\7V\254\377\7W\255\377\7W\255\377\7X\255\377\7X\256\377\7X\256\377"
  "\7Y\256\377\7Y\257\377\7Z\257\377\7Z\257\377\7Z\260\377\7[\260\377\3Y\257"
  "\377,t\275\377\365\370\373\377\315\333\354\300\346\356\366\3\347\356\366"
  "\377\322\332\353\377\0,\217\377\13""9\226\377\13""9\226\377\13:\226\377\13"
  ":\226\377\13:\227\377\13;\227\377\13;\227\377\13;\227\377\13<\230\377\13"
  "<\230\377\13=\231\377\12=\231\377\12=\231\377\23E\236\377\377\377\377\376"
  "\331\344\360\335\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\3P\242*\376\376\376\371\275\321"
  "\347\377\2Q\252\377\6V\253\377\6V\254\377\7W\255\377\7W\255\377\7W\255\377"
  "\7X\256\377\7X\256\377\7X\256\377\7Y\256\377\7Y\257\377\7Z\257\377\7Z\257"
  "\377\3X\256\377,t\274\377\365\370\373\377\315\333\354\300\346\356\366\3\347"
  "\356\366\377\322\331\352\377\0,\216\377\13""8\225\377\13""8\225\377\13""9"
  "\226\377\13""9\226\377\13:\226\377\13:\227\377\13;\227\377\13;\227\377\13"
  ";\230\377\13<\230\377\13<\230\377\13=\230\377\12<\231\377\25E\236\377\377"
  "\377\377\376\326\343\360\335\0\0\0\0\0\0\0\2\0\0\0\2\0\0\0\2\0\0\0\2\0\0"
  "\0\2\0\0\0\2\0\0\0\2\0\0\0\2\0\0\0\2\0\0\0\2\0\0\0\2\0\0\0\3\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0`\217\304\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0I\201\273\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\13}\6s\236\314=\323\340\356G\225\265\330u\243\277\336"
  "q\243\277\336q\243\277\336q\243\277\336q\243\277\336q\243\277\336q\243\277"
  "\336q\243\277\336q\243\277\336q\244\300\337o\215\260\325\216\375\375\375"
  "\373\275\320\350\377\3Q\251\377\7U\254\377\7U\254\377\6V\254\377\7V\254\377"
  "\7W\255\377\7W\255\377\7X\255\377\7X\256\377\7X\256\377\7Y\256\377\7Y\257"
  "\377\7Z\257\377\3W\256\377,s\274\377\365\370\373\377\315\333\354\300\346"
  "\356\366\3\347\356\366\377\322\331\352\377\0+\216\377\12""8\225\377\13""8"
  "\224\377\13""8\225\377\13""9\226\377\13""9\226\377\13:\226\377\13:\226\377"
  "\13:\227\377\13;\227\377\13;\230\377\13<\227\377\13<\230\377\12<\230\377"
  "\25D\234\377\377\377\377\376\366\371\373\372\353\360\367\330\353\361\367"
  "\332\353\361\367\332\353\361\367\332\353\361\367\332\353\361\367\332\353"
  "\361\367\332\353\361\367\332\353\361\367\332\353\361\367\332\354\361\367"
  "\332\353\361\367\332\345\355\365\333\346\356\364\314\324\341\357\262\316"
  "\335\354t\210\255\323'\0\0\0\0\0\0\0\0\272\316\345\0\221\262\325\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0""9t\265\0\375\377\376"
  "\0\0\0\0\0\0\0""6\7\304\327\351V\331\345\361\263\357\364\371\334\376\376"
  "\376\346\377\377\377\350\374\375\375\362\376\376\376\361\376\376\376\361"
  "\376\376\376\361\376\376\376\361\376\376\376\361\376\376\376\361\376\376"
  "\376\361\376\376\376\361\376\376\376\361\376\376\376\361\374\375\375\364"
  "\377\377\377\376\304\326\352\377\2P\251\377\10U\253\377\10T\253\377\7U\254"
  "\377\6V\253\377\6V\254\377\7W\255\377\7W\255\377\7W\255\377\7X\256\377\7"
  "X\256\377\7Y\256\377\7Y\256\377\3W\256\377,r\274\377\365\370\373\377\315"
  "\333\354\300\347\356\366\3\347\356\366\377\321\331\352\377\0+\215\377\13"
  """7\224\377\12""7\225\377\13""8\225\377\13""8\225\377\13""8\225\377\13""9"
  "\226\377\13""9\225\377\13:\226\377\13:\227\377\13;\227\377\13;\227\377\13"
  ";\227\377\12;\227\377\24D\234\377\361\364\371\377\354\360\367\377\353\357"
  "\366\377\353\357\366\377\353\357\366\377\353\357\366\377\353\357\366\377"
  "\353\357\366\377\353\357\366\377\353\357\366\377\353\357\366\377\353\357"
  "\366\377\353\357\366\377\352\357\366\377\361\364\371\377\377\377\377\377"
  "\377\377\377\374\377\377\377\365\373\374\375\352\320\337\356\274\222\262"
  "\326(\0\0\0\0\333\345\361\0\231\270\330\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\271\317\345\0\377\377\377\0\0\0\0\0\237\275\334R\341\352\363\326"
  "\377\377\376\361\377\377\377\375\355\361\370\377\271\313\345\377\227\263"
  "\331\377\207\250\323\377\205\246\322\377\205\246\322\377\205\246\322\377"
  "\205\246\322\377\205\246\322\377\205\247\323\377\205\247\323\377\205\247"
  "\323\377\205\250\323\377\205\250\323\377\205\250\323\377\205\250\323\377"
  "b\221\311\377\5R\251\377\7T\253\377\10U\252\377\7U\253\377\7U\254\377\7U"
  "\254\377\6V\254\377\7V\254\377\7W\255\377\7W\255\377\7X\256\377\7X\255\377"
  "\7X\256\377\3V\255\377,r\273\377\365\370\373\377\315\333\354\300\342\353"
  "\364\3\343\354\364\377\323\332\353\377\0*\214\377\14""6\224\377\13""7\224"
  "\377\12""7\225\377\13""8\225\377\13""7\224\377\13""8\225\377\13""9\226\377"
  "\13""9\225\377\13:\226\377\13:\227\377\13;\226\377\13;\227\377\12;\230\377"
  "\13<\230\377\25C\234\377\24D\235\377\24D\234\377\24E\235\377\24E\236\377"
  "\23E\235\377\23F\236\377\23F\236\377\23G\236\377\23G\237\377\23H\237\377"
  "\23H\237\377\23I\237\377\23I\240\377\24I\240\376\32O\243\377=j\262\377~\234"
  "\313\377\335\345\361\377\377\377\377\376\363\366\372\350\276\320\346l\0\0"
  "\0\0\356\363\370\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\351\357\366\0"
  "\0\0\0\0\313\333\354}\371\373\374\360\377\377\377\377\262\307\342\377Fx\272"
  "\377\23S\247\376\21R\250\377\17P\247\377\16P\247\377\16Q\250\377\15Q\247"
  "\377\15R\247\377\15R\250\377\15R\250\377\15S\250\377\15S\251\377\15T\251"
  "\377\15T\251\377\15U\252\377\15U\252\377\15U\252\377\13U\253\377\7S\251\377"
  "\10S\252\377\7T\253\377\10T\253\377\10U\253\377\10T\253\377\7U\254\377\6"
  "V\253\377\7V\254\377\7W\255\377\7W\254\377\7X\255\377\7W\256\377\3V\254\377"
  ",q\272\377\365\370\373\377\314\333\354\300\330\343\357\3\333\345\360\377"
  "\337\343\360\376\0(\214\377\14""6\223\377\14""6\224\377\13""7\223\377\12"
  """6\224\377\12""7\225\377\13""8\224\377\13""8\225\377\13""8\226\377\13""9"
  "\226\377\13:\225\377\13""9\226\377\13:\227\377\13;\226\377\12;\230\377\12"
  ";\227\377\12;\230\377\12<\231\377\12=\230\377\12=\231\377\12=\232\377\12"
  ">\231\377\11>\232\377\11?\233\377\11?\232\377\11?\232\377\11@\233\377\11"
  "@\233\377\11@\233\377\11A\234\377\11A\234\377\12C\235\377\13D\236\376\20"
  "I\240\377p\222\306\377\372\372\374\377\370\372\373\356\312\332\352m\0\0\0"
  "\0\256\306\341\0\0\0\0\0\0\0\0\0\0\0\0\0\270\316\344\0\0\0\0\0\323\340\356"
  "w\373\374\375\362\360\363\370\377N}\274\377\14L\244\377\11K\244\377\10J\243"
  "\377\10L\244\377\10K\245\377\10L\244\377\10M\244\377\10M\245\377\10N\246"
  "\377\7M\245\377\7N\246\377\7N\247\377\7O\246\377\7P\247\377\7O\250\377\7"
  "P\247\377\7P\247\377\7Q\250\377\7Q\251\377\10R\251\377\10S\252\377\10S\252"
  "\377\10T\253\377\7T\253\377\10U\252\377\10T\253\377\7U\254\377\6V\253\377"
  "\7V\254\377\7W\255\377\7V\254\377\7W\255\377\3U\254\377/s\272\377\365\370"
  "\373\377\305\326\351\301\335\347\361\3\340\351\363\350\351\354\364\377\10"
  """1\221\377\13""4\222\377\13""6\223\377\14""6\224\377\14""7\223\377\13""6"
  "\224\377\12""7\225\377\12""7\224\377\13""8\225\377\13""9\226\377\13""8\225"
  "\377\13""9\225\377\13""9\226\377\13:\227\377\13;\226\377\13;\230\377\13<"
  "\227\377\13<\230\377\13=\231\377\13=\230\377\12>\231\377\13=\232\377\12>"
  "\231\377\11?\231\377\12>\232\377\11?\233\377\12?\232\377\12@\233\377\12A"
  "\234\377\12@\233\377\12A\234\377\12A\235\377\12B\234\377\11B\235\377\12D"
  "\236\377Fq\265\376\365\367\372\377\363\367\372\343\235\273\332M\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\252\303\337M\371\372\374\347\360\363\371\377"
  "4i\262\376\10H\242\377\10I\242\377\11J\242\377\11K\243\377\11K\244\377\11"
  "L\244\377\11K\245\377\11L\244\377\10M\244\377\11M\245\377\11N\246\377\10"
  "M\245\377\7N\246\377\7N\247\377\10O\246\377\10P\247\377\10O\250\377\10P\250"
  "\377\10P\247\377\10Q\250\377\10Q\250\377\10R\251\377\10S\252\377\10S\251"
  "\377\10T\252\377\10T\253\377\7U\252\377\10T\253\377\7U\254\377\6V\253\377"
  "\7U\253\377\6V\254\377\7V\254\377\2T\254\377?}\277\377\366\370\373\377\325"
  "\341\356\240\330\343\360\2\334\346\362\306\355\357\366\377+N\237\377\10""1"
  "\221\377\14""5\222\377\14""6\223\377\13""5\224\377\14""6\223\377\13""6\224"
  "\377\12""7\224\377\12""7\224\377\12""7\224\377\13""8\225\377\13""8\226\377"
  "\13""9\225\377\13""9\226\377\13:\226\377\13:\226\377\13;\227\377\13;\230"
  "\377\13;\227\377\13<\230\377\13<\230\377\12=\231\377\13=\232\377\13>\231"
  "\377\12>\231\377\11>\232\377\11?\233\377\11?\232\377\12@\233\377\12A\234"
  "\377\12@\233\377\12A\234\377\12A\234\377\12B\235\377\11B\235\377\14D\236"
  "\377]\204\277\377\377\377\377\377\345\354\365\272\0\0g\10\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\35\3\356\363\370\273\377\377\377\377S\177\275\377\12I\242\377"
  "\10H\241\377\11H\242\377\11I\242\377\11J\242\377\11J\243\377\11J\244\377"
  "\11K\243\377\11K\244\377\11L\245\377\11L\244\377\10L\245\377\11M\245\377"
  "\10M\245\377\7N\245\377\7N\246\377\7N\246\377\10O\247\377\10O\247\377\10"
  "P\250\377\10P\247\377\10Q\250\377\10Q\250\377\10R\251\377\10S\252\377\10"
  "R\251\377\10S\252\377\10S\253\377\7T\252\377\10U\253\377\10T\253\377\7U\253"
  "\377\6U\253\377\6V\254\377\0R\252\377]\222\311\377\367\371\373\377\326\342"
  "\357\177\306\327\351\2\316\335\354\224\363\364\371\377]w\266\376\4-\216\377"
  "\14""4\222\377\14""4\222\377\14""5\222\377\13""5\223\377\14""6\223\377\13"
  """6\223\377\13""7\224\377\13""7\225\377\12""7\224\377\13""8\225\377\13""8"
  "\225\377\13""9\225\377\13""9\225\377\13""9\226\377\13:\227\377\13:\227\377"
  "\13;\227\377\13;\227\377\13<\230\377\13<\230\377\13=\230\377\12=\231\377"
  "\13=\232\377\12>\231\377\12>\232\377\12?\232\377\11?\232\377\12@\233\377"
  "\12@\233\377\12@\233\377\12@\233\377\12A\234\377\12B\234\377\7A\234\377\35"
  "Q\245\376\261\303\340\377\367\372\373\352\261\311\342`\0\0\0\0\200\246\320"
  "\0\0\0\0\0\314\332\353U\377\377\377\346\257\303\340\377\27R\246\376\7F\240"
  "\377\11G\241\377\11H\241\377\11H\241\377\11I\242\377\11I\243\377\11J\243"
  "\377\11J\243\377\11K\243\377\11K\244\377\11K\245\377\11L\244\377\10L\244"
  "\377\11M\245\377\10M\246\377\10N\245\377\10N\246\377\7N\246\377\10O\246\377"
  "\10O\247\377\10O\250\377\10P\247\377\10Q\250\377\10Q\250\377\10R\251\377"
  "\10R\251\377\10R\251\377\10S\252\377\10S\252\377\10T\252\377\7U\252\377\10"
  "T\253\377\7U\254\377\7U\253\377\0O\251\377\220\263\332\376\372\373\374\377"
  "\245\300\335]\225\264\330\0\257\306\341O\370\371\373\377\241\260\324\376"
  "\0*\214\377\13""4\221\377\14""4\222\377\14""4\222\377\14""5\222\377\13""5"
  "\223\377\14""6\223\377\14""6\223\377\13""6\224\377\12""7\225\377\12""7\224"
  "\377\12""7\224\377\13""8\225\377\13""8\226\377\13""9\225\377\13:\226\377"
  "\13:\226\377\13:\227\377\13;\227\377\13;\227\377\13<\230\377\13<\230\377"
  "\13=\230\377\12=\231\377\13=\231\377\13>\231\377\12>\231\377\11>\232\377"
  "\11?\232\377\12?\232\377\12@\233\377\12A\234\377\12@\233\377\12A\234\377"
  "\11A\234\377\10A\234\377U|\273\377\377\377\376\377\344\354\364\226\0\0\0"
  "\0\0\0\0\0\0\0\0\0\345\355\365\234\377\377\377\377Oz\272\377\11G\240\377"
  "\10F\240\377\11G\240\377\11G\241\377\11H\241\377\11H\242\377\11I\242\377"
  "\11I\242\377\11J\242\377\11J\243\377\11K\243\377\11K\244\377\11K\244\377"
  "\11L\244\377\10L\244\377\11M\245\377\11M\246\377\10M\245\377\7N\246\377\7"
  "N\246\377\10N\246\377\10O\247\377\10O\247\377\10P\247\377\10Q\250\377\10"
  "Q\250\377\10Q\250\377\10R\251\377\10R\251\377\10S\251\377\10S\252\377\10"
  "T\252\377\7T\252\377\10T\253\377\7T\253\377\0Q\251\377\321\337\356\377\363"
  "\367\372\377U\210\300\36\0\0\0\0\312\332\353\12\352\360\367\360\342\346\361"
  "\377\40D\233\376\7""0\217\377\14""3\222\377\14""4\222\377\14""4\222\377\14"
  """4\222\377\14""5\222\377\13""5\223\377\14""6\223\377\13""6\224\377\13""7"
  "\224\377\12""7\225\377\12""7\224\377\13""8\225\377\13""8\226\377\13""9\226"
  "\377\13""9\226\377\13:\226\377\13:\227\377\13;\227\377\13;\227\377\13;\227"
  "\377\13<\230\377\13<\230\377\13=\230\377\12=\231\377\13=\231\377\12>\231"
  "\377\11>\232\377\11?\233\377\11?\232\377\12@\233\377\12@\233\377\12A\234"
  "\377\12A\234\377\5>\233\3771a\254\376\333\343\360\377\364\370\373\275\\\216"
  "\304\34\0\0\0\0,k\257\25\367\372\374\303\337\347\362\377*^\254\376\6D\236"
  "\377\11F\240\377\10F\240\377\10G\240\377\11G\240\377\11H\241\377\11H\241"
  "\377\11I\242\377\11I\242\377\11J\242\377\11J\243\377\11J\243\377\11K\243"
  "\377\11K\244\377\11K\244\377\11L\244\377\10L\245\377\11M\245\377\10M\245"
  "\377\7N\245\377\7N\246\377\10N\246\377\10O\246\377\10P\247\377\10P\250\377"
  "\10P\250\377\10Q\250\377\10Q\250\377\10R\251\377\10R\251\377\10R\251\377"
  "\10S\252\377\10S\252\377\10T\253\377\0N\247\377M\204\303\376\364\367\373"
  "\377\320\335\354\310\377\377\377\0\0\0\0\0\324\341\357\4\332\345\361{\364"
  "\365\371\377\222\243\315\376\7.\216\377\11""1\220\377\14""3\222\377\14""4"
  "\221\377\14""4\222\377\14""4\222\377\14""5\222\377\13""5\223\377\14""6\223"
  "\377\14""6\223\377\13""6\224\377\12""7\225\377\12""7\224\377\13""8\225\377"
  "\13""8\225\377\13""9\225\377\13""9\226\377\13:\226\377\13:\226\377\13:\227"
  "\377\13;\227\377\13;\227\377\13<\230\377\13<\230\377\13=\230\377\12=\231"
  "\377\13=\232\377\12>\231\377\12>\232\377\11>\232\377\12?\232\377\12@\232"
  "\377\12@\233\377\12@\233\377\7?\233\377!S\246\377\253\276\335\377\371\373"
  "\374\327\264\312\342A\0\0\0\0\271\316\344;\376\377\376\326\254\277\336\377"
  "\36U\247\377\10D\237\377\11E\237\377\11F\237\377\10F\240\377\11G\241\377"
  "\11G\240\377\11H\241\377\11H\241\377\11H\242\377\11I\242\377\11I\243\377"
  "\11J\243\377\11J\243\377\11K\243\377\11K\244\377\11K\244\377\11L\244\377"
  "\10L\244\377\11M\245\377\10M\246\377\10M\245\377\7N\246\377\10N\246\377\10"
  "O\247\377\10O\247\377\10P\250\377\10P\247\377\10Q\250\377\10Q\250\377\10"
  "Q\250\377\10R\251\377\10R\251\377\10S\252\377\1O\250\377\25]\257\376\304"
  "\326\352\377\367\371\373\377\246\300\335U\216\257\325\1\0\0\0\0\0\0\0\0\254"
  "\305\340\33\346\355\365\342\356\361\366\377_x\266\376\11""0\220\377\6.\217"
  "\377\13""3\221\377\14""3\221\377\14""4\222\377\14""4\222\377\14""5\222\377"
  "\14""5\223\377\13""5\223\377\14""6\223\377\13""6\224\377\12""7\224\377\12"
  """7\224\377\13""8\224\377\13""8\225\377\13""9\226\377\13""9\226\377\13:\226"
  "\377\13:\226\377\13:\227\377\13;\227\377\13;\230\377\13;\227\377\13<\230"
  "\377\13<\230\377\12=\231\377\13=\231\377\13>\231\377\12>\231\377\11>\232"
  "\377\11?\233\377\12@\233\377\12@\233\377\10?\233\377\24H\240\377\212\244"
  "\317\377\374\376\376\351\310\330\352Z\0\0\0\0\315\334\354W\377\377\377\345"
  "\211\245\320\377\25M\243\377\10D\236\377\12E\237\377\12E\240\377\11F\237"
  "\377\10F\240\377\11G\241\377\11G\240\377\11H\241\377\11H\241\377\11H\242"
  "\377\11I\242\377\11I\242\377\11J\242\377\11J\243\377\11J\243\377\11K\243"
  "\377\11K\244\377\11L\244\377\10L\244\377\11L\245\377\11M\245\377\10M\245"
  "\377\7N\245\377\10N\246\377\10O\247\377\10O\247\377\10P\247\377\10P\250\377"
  "\7P\250\377\7Q\250\377\7Q\250\377\4O\247\377\0K\246\377\36b\261\376\235\273"
  "\335\377\366\370\373\377\330\344\360\254C|\271\6\0\0\0\0\225\266\327\0\0"
  "\0\0\0n\232\310\2\274\321\345R\363\367\372\355\351\353\364\377s\210\277\376"
  "\"E\233\376\2*\214\377\3+\215\377\6.\217\377\7""0\220\377\7""0\220\377\7"
  """0\220\377\7""1\220\377\7""2\221\377\7""2\221\377\6""3\221\377\6""3\222"
  "\377\6""3\223\377\7""4\223\377\10""6\224\377\12""8\225\377\13""9\226\377"
  "\13""9\226\377\13:\226\377\13:\227\377\13:\227\377\13;\227\377\13;\227\377"
  "\13<\230\377\13<\230\377\13=\230\377\12=\231\377\13=\232\377\12>\231\377"
  "\12?\232\377\11?\232\377\12?\233\377\11@\233\377\14B\234\377u\223\307\377"
  "\377\377\377\360\340\351\363c\0\0\0\0\305\327\351r\376\377\376\363r\224\307"
  "\377\17G\240\377\11C\236\377\12D\236\377\11E\237\377\12E\240\377\11F\237"
  "\377\10F\240\377\10G\240\377\11G\241\377\11G\241\377\11H\241\377\11H\241"
  "\377\11I\242\377\11I\243\377\4F\241\377\0B\237\377\0B\237\377\0C\237\377"
  "\0C\240\377\0C\240\377\0D\240\377\0D\240\377\0E\241\377\0E\242\377\0F\242"
  "\377\0F\242\377\0G\242\377\0G\243\377\0G\243\377\0H\243\377\0H\244\377\2"
  "M\246\377\37b\261\376R\206\303\376\261\311\343\377\374\375\375\377\333\346"
  "\361\315\212\256\324$\0\0\0\0\0\0\0\0\0\0\0\0\263\312\342\0\0\0\0\0\211\260"
  "\322\1\313\332\353_\360\365\371\337\363\365\371\377\266\301\335\377x\215"
  "\301\376Pl\260\3779Y\246\377.Q\241\377,P\240\377,P\241\377,P\241\377,P\241"
  "\377,P\242\377-R\242\377-Q\242\377*P\242\377\"K\237\377\24>\231\377\0""0"
  "\220\377\0/\220\377\6""5\224\377\13""9\226\377\13:\226\377\13:\226\377\13"
  ":\227\377\13;\227\377\13;\227\377\13<\230\377\13<\230\377\13=\230\377\12"
  "=\231\377\13>\231\377\13>\232\377\12>\231\377\11?\232\377\11?\232\377\7>"
  "\232\377i\212\301\377\377\377\377\357\371\372\374b\0\0\0\0\337\351\363o\377"
  "\377\377\362f\212\302\377\13D\236\377\11C\235\377\12D\236\377\12D\236\377"
  "\11E\237\377\12E\237\377\11F\240\377\11F\237\377\10G\240\377\11G\241\377"
  "\11G\241\377\11H\241\377\11H\241\377\4E\240\3774i\263\377f\217\305\377i\221"
  "\306\377i\221\307\377i\221\307\377i\221\307\377i\221\310\377i\221\310\377"
  "i\222\310\377i\222\310\377h\223\310\377h\223\310\377h\223\310\377h\223\311"
  "\377h\223\311\377j\225\311\377u\235\316\376\216\257\327\377\264\312\344\377"
  "\355\362\370\377\371\373\374\367\345\354\364\246\211\256\3231\0\0\0\0\0\0"
  "\0\0\214\260\325\0\0\0\0\0\0\0\0\0\307\330\352\0\0\0\0\0\0\0\0\0\300\322"
  "\347B\351\360\366\234\364\370\373\360\364\366\372\377\342\346\361\377\316"
  "\326\350\377\305\316\344\377\303\315\344\377\303\315\344\377\303\315\343"
  "\377\303\315\344\377\303\315\344\377\303\315\344\377\303\315\344\377\302"
  "\314\343\377\275\311\342\377\260\276\334\377\230\254\322\377i\205\275\376"
  "%N\241\377\1""1\221\377\12""9\226\377\13:\226\377\13:\226\377\13:\227\377"
  "\13;\227\377\13;\227\377\13;\227\377\13<\230\377\13<\231\377\12=\230\377"
  "\13>\231\377\13=\232\377\12>\231\377\11>\232\377\6<\231\377d\206\300\377"
  "\377\377\377\357\376\376\376b\0\0\0\0\346\355\365o\377\377\377\362c\207\300"
  "\377\13C\235\377\11B\235\377\12C\235\377\12C\236\377\12D\237\377\12E\236"
  "\377\12E\237\377\12F\240\377\11F\240\377\10F\240\377\11G\240\377\11G\241"
  "\377\11H\241\377\0?\235\377s\227\312\377\355\361\367\376\367\371\373\377"
  "\367\371\373\377\367\371\373\377\367\370\373\377\367\370\373\377\367\370"
  "\373\377\367\370\373\377\367\370\373\377\367\371\373\377\367\371\373\377"
  "\367\371\373\377\367\371\373\377\367\371\373\377\370\371\374\377\367\371"
  "\374\377\364\367\372\377\357\363\370\345\343\353\364\252\330\344\360_s\236"
  "\313\24\0\0\0\0\0\0\0\0\204\252\321\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\271"
  "\315\345\0\0\0\0\0\0\0\0\0\244\300\335\17\323\340\356D\341\352\363q\335\350"
  "\362\243\353\362\370\270\377\377\377\266\377\377\377\266\377\377\377\266"
  "\377\377\377\266\377\377\377\266\377\377\377\266\377\377\377\266\377\377"
  "\377\265\377\377\377\270\343\355\365\341\361\366\372\353\364\370\373\377"
  "\361\363\370\377\300\313\343\377Qq\263\376\4""3\222\377\12""9\225\377\13"
  """9\226\377\13:\226\377\13:\226\377\13:\227\377\13;\230\377\13;\227\377\13"
  "<\230\377\13=\231\377\12=\231\377\13>\231\377\13>\232\377\12>\231\377\6<"
  "\230\377e\206\277\377\377\377\377\357\376\376\376b\0\0\0\0\346\355\365o\377"
  "\377\377\362c\206\300\377\13C\235\377\11B\235\377\12C\236\377\12D\235\377"
  "\12D\236\377\12D\237\377\11E\237\377\12E\237\377\11F\240\377\11F\240\377"
  "\10F\240\377\10G\240\377\11G\241\377\0>\234\377y\233\313\377\373\373\375"
  "\377\332\345\361\252\266\314\343\203\272\316\344\207\272\316\344\207\272"
  "\316\344\207\272\316\344\207\272\316\344\207\272\316\344\207\272\316\344"
  "\207\272\316\344\207\272\316\344\207\272\316\344\207\270\315\344\210\304"
  "\325\351}\346\355\365`\336\347\362Q\305\326\351=\204\251\321\31\0\0\0\0\0"
  "\0\0\0\0\0\0\0{\243\315\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\254\304\340\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0#\1\211\254\323\27\276\322\347"
  "\37\355\362\370\36\366\370\373\36\366\370\373\36\366\370\373\36\366\370\373"
  "\36\366\370\373\36\366\370\373\36\366\371\373\36\364\370\372\40\273\320\346"
  "0\325\341\3574\325\341\357L\336\350\363\232\363\367\372\374\306\317\345\377"
  "6Z\247\377\5""4\223\377\13""9\226\377\13""9\226\377\13:\226\377\13:\227\377"
  "\13;\227\377\13;\230\377\13<\230\377\13<\230\377\13<\230\377\12=\231\377"
  "\13=\231\377\12=\231\377\6;\230\377e\206\277\377\377\377\377\357\376\376"
  "\376b\0\0\0\0\346\355\365o\377\377\377\362c\206\277\377\13B\235\377\11A\234"
  "\377\12B\235\377\12C\236\377\12C\236\377\12D\236\377\12D\236\377\11D\237"
  "\377\12E\237\377\11E\237\377\11F\240\377\11F\240\377\10G\240\377\0=\234\377"
  "x\232\313\377\370\371\373\377\336\351\362T\0\0\0\4\0\13}\13\0\13}\13\0\13"
  "}\13\0\13}\13\0\13}\13\0\13}\13\0\13}\13\0\13}\13\0\13}\13\0\13}\13\0\15"
  "\177\13\0\0_\7\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\215\260\325\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\212\255\322\0\200\247\321\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\16V\247\10\346\356\365\203\363\365\371\377i\204\275\377\0/\220\377"
  "\13""8\225\377\13""9\226\377\13""9\226\377\13:\226\377\13:\226\377\13:\227"
  "\377\13;\227\377\13;\230\377\13<\227\377\13<\230\377\12=\231\377\12=\231"
  "\377\7;\230\377e\205\277\377\377\377\377\357\376\376\376b\0\0\0\0\346\355"
  "\365o\377\377\377\362c\206\277\377\13B\234\377\11@\233\377\12B\234\377\12"
  "B\235\377\12C\235\377\12C\236\377\12D\235\377\12D\236\377\11D\237\377\12"
  "E\237\377\12E\237\377\11F\240\377\11F\240\377\0=\233\377x\232\313\377\366"
  "\370\373\377\377\377\377?\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\224\263\330\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\204\251\321\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\1`\221\305'\217\260\325I\256\306\340T\275\321\346S\275\321\346S\275\321"
  "\346S\275\321\346S\275\321\346S\275\321\346S\275\321\346S\275\321\346S\275"
  "\321\346S\275\321\346S\275\321\346S\265\314\343L\335\347\361\222\367\372"
  "\373\377t\215\302\377\0.\217\377\13""8\225\377\13""8\225\377\13""8\225\377"
  "\13""9\226\377\13""9\226\377\13:\226\377\13;\227\377\13;\227\377\13;\230"
  "\377\13<\227\377\13<\230\377\12<\230\377\7:\227\377f\205\277\377\377\377"
  "\377\357\376\376\376b\0\0\0\0\346\355\365o\377\377\377\362c\206\277\377\13"
  "A\234\377\11@\233\377\12A\234\377\12B\235\377\12B\234\377\12C\235\377\12"
  "C\236\377\12C\236\377\12D\236\377\12D\236\377\11E\237\377\12E\237\377\11"
  "F\240\377\0<\233\377x\231\312\377\372\373\374\377\341\353\364\333\316\335"
  "\355\312\317\336\355\313\317\336\355\313\317\336\355\313\317\336\355\313"
  "\317\336\355\313\317\336\355\313\317\336\355\313\317\336\355\313\317\336"
  "\355\313\317\336\355\313\320\336\355\313\312\332\353\316\321\337\355\270"
  "\322\337\356\227\267\314\344o\211\255\323$\0\0\0\0\0\0\0\0\0\0\0\0\215\257"
  "\324\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\204\251\322\0\0\0\0\0\0\0\0\0\0\0"
  "V\4\241\275\334O\335\347\362\247\350\356\366\343\371\373\374\357\377\377"
  "\377\361\377\377\377\362\377\377\377\362\377\377\377\362\377\377\377\362"
  "\377\377\377\362\377\377\377\362\377\377\377\362\377\377\377\362\377\377"
  "\377\362\377\377\377\362\377\377\377\362\377\377\377\362\377\377\377\362"
  "\377\377\377\367\377\377\377\377w\217\303\377\0-\216\377\12""7\224\377\12"
  """8\225\377\13""8\224\377\13""8\225\377\13""9\226\377\13""9\226\377\13:\226"
  "\377\13:\227\377\13;\227\377\13;\230\377\13<\230\377\12;\227\377\7""9\226"
  "\377f\205\277\377\377\377\377\357\376\376\376b\0\0\0\0\346\355\365o\377\377"
  "\377\362c\205\277\377\13@\233\377\11@\233\377\12A\234\377\12A\234\377\12"
  "B\235\377\12B\235\377\12C\235\377\12C\235\377\12C\236\377\12D\235\377\12"
  "D\236\377\11D\237\377\12E\237\377\0;\232\377|\234\314\377\377\377\377\376"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\375\377\377\377\371\377\377\377\366\373\374\375\355"
  "\322\337\356\312\224\264\327J\0\0\0\0\0\0\0\0\377\377\377\0\0L\241\0\0\0"
  "\0\0\241\275\335\0\0\0\0\0\0\0\0\0\235\273\333;\325\342\357\322\373\375\375"
  "\375\377\377\377\374\364\366\371\377\313\322\346\377\247\264\326\377\226"
  "\246\316\377\221\241\314\377\220\240\314\377\220\241\314\377\220\241\314"
  "\377\220\241\314\377\220\241\315\377\220\242\315\377\220\242\315\377\220"
  "\242\315\377\220\242\315\377\220\243\314\377\220\243\315\377\221\243\316"
  "\377\220\243\315\376Ee\254\377\5""1\221\377\14""7\223\377\13""6\224\377\12"
  """7\225\377\13""7\224\377\13""8\225\377\13""9\226\377\13""9\225\377\13:\226"
  "\377\13:\227\377\13;\227\377\13;\227\377\12:\227\377\7""9\226\377f\204\276"
  "\377\377\377\377\357\375\375\375b\0\0\0\0\346\355\365o\377\377\377\362c\205"
  "\277\377\13?\233\377\11?\232\377\12@\232\377\12@\233\377\12A\234\377\12A"
  "\234\377\12B\235\377\12B\234\377\12C\235\377\12C\236\377\12D\235\377\12C"
  "\236\377\12D\237\377\10C\236\377\27O\244\377([\253\376(\\\253\377']\253\377"
  "(]\254\377(]\254\377(^\254\377(^\254\377(^\255\377(_\255\377(_\255\377(`"
  "\256\377(`\256\377(a\256\377'a\257\377(a\257\3773i\263\377R\200\277\377\210"
  "\250\323\377\330\343\360\377\377\376\376\376\373\374\375\372\314\333\354"
  "\216Q\206\300\14\0\0\0\0\0\0\0\0\215\260\325\0\0\0\0\0\200\246\321\4\272"
  "\316\345]\354\361\367\377\374\374\375\377\304\314\344\377Un\261\377\26""8"
  "\223\377\12-\216\376\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""1\220\377\14""1\220\377\14""2\220\377\14""2\220\377"
  "\14""2\221\377\14""3\221\377\14""3\221\377\14""4\222\377\14""4\222\377\14"
  """4\222\377\14""5\223\377\14""5\223\377\13""6\222\377\14""5\223\377\14""6"
  "\223\377\13""6\224\377\12""7\225\377\13""8\225\377\13""7\224\377\13""8\225"
  "\377\13""9\225\377\13:\226\377\13:\227\377\13;\226\377\12:\227\377\7""8\226"
  "\377f\204\276\377\377\377\377\357\375\375\375b\0\0\0\0\346\355\365o\377\377"
  "\377\362d\204\277\377\13?\232\377\11>\232\377\12?\233\377\12?\232\377\12"
  "@\233\377\12@\233\377\12A\234\377\12B\235\377\12B\234\377\12C\235\377\12"
  "B\236\377\12C\236\377\12D\235\377\11C\236\377\12D\237\377\12E\236\377\12"
  "E\237\377\12F\240\377\11F\237\377\11G\240\377\11G\241\377\11H\241\377\11"
  "H\241\377\11I\242\377\11I\242\377\11I\243\377\11J\243\377\11J\243\377\11"
  "K\243\377\11K\244\377\11K\243\377\11L\244\377\10K\244\376\11L\245\377V\205"
  "\301\377\336\347\362\377\377\377\377\377\277\321\346\264\255\304\337\7\0"
  "\0\0\0\0\0\0\0n\232\311\3\257\307\341T\363\367\372\377\353\355\365\377n\203"
  "\273\376\5*\213\377\11-\215\377\13/\216\377\14""0\217\377\14""0\216\377\13"
  "/\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14/\216\377\14""0\216"
  "\377\14""0\217\377\14""1\217\377\14""1\220\377\14""2\221\377\14""3\220\377"
  "\14""2\221\377\14""3\222\377\14""3\221\377\14""4\222\377\14""5\223\377\14"
  """4\222\377\14""5\222\377\13""5\223\377\14""6\224\377\13""6\223\377\13""7"
  "\224\377\13""7\225\377\12""7\224\377\13""8\225\377\13""8\225\377\13:\225"
  "\377\13""9\226\377\12""9\226\377\7""8\225\377f\203\276\377\377\377\377\357"
  "\376\376\376b\0\0\0\0\346\355\365o\377\377\377\362d\204\276\377\14>\232\377"
  "\11>\231\377\12>\232\377\12?\232\377\11?\232\377\12@\233\377\12@\233\377"
  "\12@\233\377\12A\234\377\12A\235\377\12B\234\377\12C\235\377\12C\236\377"
  "\12D\235\377\12C\236\377\12D\237\377\12D\236\377\11E\237\377\12E\240\377"
  "\11F\237\377\10F\237\377\10F\240\377\11G\241\377\11G\241\377\11H\242\377"
  "\11I\241\377\11I\242\377\11J\243\377\11I\242\377\11J\243\377\11J\244\377"
  "\11K\243\377\11L\244\377\11K\245\377\3H\242\377\37]\255\377\314\331\354\376"
  "\374\375\375\377\312\332\353r\243\277\335\1>y\270\0\220\262\326$\356\363"
  "\370\360\353\355\365\377\\t\263\376\0$\210\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14/\217\377\14/\216\377\14""0\217"
  "\377\14""1\220\377\14""1\217\377\14""1\220\377\14""2\220\377\14""2\221\377"
  "\14""3\221\377\14""3\221\377\14""4\221\377\14""4\222\377\14""4\223\377\14"
  """5\222\377\13""5\223\377\14""6\223\377\14""6\223\377\13""6\224\377\12""7"
  "\224\377\13""7\224\377\13""7\224\377\13""8\225\377\13""9\225\377\12""9\226"
  "\377\7""7\225\377f\203\275\377\377\377\377\357\376\376\376b\0\0\0\0\346\355"
  "\365o\377\377\377\362d\203\276\377\14>\232\377\12=\231\377\13>\231\377\12"
  ">\231\377\11>\232\377\12?\232\377\12@\233\377\12@\233\377\12@\233\377\12"
  "A\234\377\12A\235\377\12B\234\377\12C\235\377\12B\235\377\12C\235\377\12"
  "C\235\377\12D\236\377\12D\236\377\11E\237\377\12E\237\377\11E\240\377\11"
  "F\237\377\10F\240\377\11F\241\377\11G\241\377\11H\241\377\11I\241\377\11"
  "H\242\377\11I\242\377\11I\242\377\11J\242\377\11J\243\377\11K\243\377\11"
  "K\244\377\11L\245\377\3H\242\377*d\261\377\346\354\365\377\353\361\367\377"
  "\210\254\322\35\333\346\360\1\305\327\351\241\372\372\374\377\213\234\311"
  "\376\0!\207\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14/\216\377\14""0\217"
  "\377\14""1\217\377\14""1\217\377\14""1\220\377\14""2\220\377\14""2\220\377"
  "\14""3\221\377\14""3\222\377\14""3\221\377\14""4\222\377\14""4\223\377\14"
  """4\222\377\14""5\222\377\13""5\223\377\14""6\223\377\13""6\223\377\12""7"
  "\224\377\12""8\225\377\13""8\225\377\13""8\225\377\12""8\225\377\7""6\224"
  "\377f\203\275\377\377\377\377\357\376\376\376b\0\0\0\0\346\355\365o\377\377"
  "\377\362d\203\276\377\14=\231\377\12=\230\377\12=\231\377\13=\232\377\12"
  ">\231\377\11>\232\377\12?\233\377\12@\233\377\12@\233\377\12@\233\377\12"
  "A\234\377\12A\234\377\12B\234\377\12B\234\377\12B\235\377\12C\236\377\12"
  "C\235\377\12C\236\377\12D\236\377\11D\236\377\12E\237\377\12E\240\377\11"
  "F\237\377\10F\240\377\11G\240\377\11G\240\377\11H\241\377\11H\241\377\11"
  "H\241\377\11I\242\377\11I\243\377\11J\242\377\11J\243\377\11J\243\377\11"
  "K\243\377\11L\244\377\0C\240\377}\241\320\376\373\374\375\377\302\324\347"
  "\225\233\271\331\20\347\356\366\377\337\344\360\377\23""6\222\377\12.\216"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14/\216\377\14/\216\377\14""0\217"
  "\377\14""0\217\377\14""1\217\377\14""1\220\377\14""2\220\377\14""2\220\377"
  "\14""2\221\377\14""3\221\377\14""3\221\377\14""4\222\377\14""5\222\377\14"
  """4\222\377\14""5\222\377\13""5\223\377\14""6\224\377\13""6\223\377\13""7"
  "\224\377\12""7\225\377\13""8\225\377\12""8\225\377\7""6\224\377f\203\274"
  "\377\377\377\377\357\376\376\376b\0\0\0\0\346\355\365o\377\377\377\362d\203"
  "\275\377\14=\231\377\12<\230\377\13=\230\377\12>\231\377\13=\232\377\12>"
  "\231\377\11?\232\377\11?\232\377\12?\233\377\12@\233\377\12@\233\377\12@"
  "\233\377\12A\234\377\12A\234\377\12B\234\377\12B\235\377\12C\235\377\12D"
  "\235\377\12C\236\377\12D\237\377\12D\236\377\11E\237\377\12E\240\377\11E"
  "\237\377\10F\237\377\10G\240\377\11G\240\377\11G\241\377\11H\241\377\11H"
  "\241\377\11H\242\377\11I\242\377\11I\242\377\11J\243\377\11J\243\377\11K"
  "\243\377\7J\244\377\27V\252\377\356\362\370\377\337\350\362\353\247\301\336"
  "d\375\375\375\377\225\245\315\376\0%\211\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14/\217\377\14/\216\377\14""0\217\377"
  "\14""0\217\377\14""1\217\377\14""1\220\377\14""2\220\377\14""2\220\377\14"
  """2\221\377\14""3\221\377\14""3\221\377\14""4\221\377\14""4\222\377\14""4"
  "\223\377\14""5\222\377\13""5\223\377\14""6\223\377\14""6\223\377\13""7\224"
  "\377\12""7\225\377\12""7\224\377\7""5\224\377f\202\274\377\377\377\377\357"
  "\376\376\376b\0\0\0\0\346\355\365o\377\377\377\362d\202\275\377\14<\230\377"
  "\12<\230\377\13<\230\377\13=\230\377\12=\231\377\13=\232\377\12>\231\377"
  "\12>\232\377\11?\232\377\12?\232\377\12@\233\377\12@\233\377\12@\233\377"
  "\12A\234\377\12A\234\377\12B\234\377\12B\235\377\12C\235\377\12C\235\377"
  "\12C\236\377\12D\236\377\12D\236\377\12E\237\377\12E\237\377\11F\240\377"
  "\11F\240\377\10G\240\377\11G\241\377\11G\241\377\11H\241\377\11H\241\377"
  "\11H\242\377\11I\242\377\11I\242\377\11J\242\377\11J\243\377\11K\243\377"
  "\0B\237\377\277\320\346\376\373\373\375\377\320\335\355\242\367\367\372\377"
  "Ni\255\377\6+\214\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14/\216\377\14""0\217\377\14""0\217"
  "\377\14""1\217\377\14""1\217\377\14""1\220\377\14""2\220\377\14""2\220\377"
  "\14""3\221\377\14""3\221\377\14""3\221\377\14""4\222\377\14""4\223\377\14"
  """4\222\377\13""5\222\377\14""6\223\377\14""6\224\377\13""7\224\377\12""7"
  "\224\377\7""4\223\377f\201\274\377\377\377\377\357\376\376\376b\0\0\0\0\346"
  "\355\365o\377\377\377\362d\202\274\377\14<\230\377\12;\227\377\13;\227\377"
  "\13<\230\377\13<\230\377\13=\231\377\13=\231\377\12>\232\377\12>\232\377"
  "\11?\232\377\11?\233\377\12@\233\377\12@\233\377\12@\233\377\12A\234\377"
  "\12A\234\377\12B\234\377\12B\235\377\12B\235\377\12C\235\377\12C\235\377"
  "\12C\236\377\11D\236\377\12E\236\377\12E\237\377\11F\240\377\11F\240\377"
  "\10F\240\377\10G\240\377\11G\240\377\11H\241\377\11H\241\377\11H\242\377"
  "\11I\242\377\11I\242\377\11J\242\377\11J\243\377\1E\241\377\210\250\322\377"
  "\374\375\375\377\334\346\361\314\364\365\371\377\40A\230\377\11.\215\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14/\216\377\14""0\216\377\14""0\217\377"
  "\14""1\217\377\14""1\217\377\14""1\220\377\14""2\220\377\14""2\220\377\14"
  """2\221\377\14""3\221\377\14""3\221\377\14""4\222\377\14""4\222\377\14""4"
  "\223\377\13""5\222\377\14""6\223\377\13""6\223\377\13""6\223\377\7""4\223"
  "\377e\201\274\377\377\377\377\357\376\376\376b\0\0\0\0\346\355\365o\377\377"
  "\377\362d\202\275\377\14;\227\377\12:\227\377\13;\230\377\13;\227\377\13"
  "<\230\377\13=\230\377\12=\231\377\13=\231\377\12>\232\377\12>\232\377\12"
  "?\232\377\11?\232\377\12@\233\377\12@\233\377\12@\233\377\12@\233\377\12"
  "A\234\377\12A\234\377\12B\234\377\12B\235\377\12C\236\377\12C\235\377\12"
  "C\236\377\12D\236\377\11E\237\377\12E\237\377\11F\240\377\11F\237\377\11"
  "F\240\377\10G\240\377\11G\240\377\11G\241\377\11H\241\377\11H\241\377\11"
  "H\242\377\11I\242\377\11I\242\377\3F\241\377g\220\306\377\373\374\375\377"
  "\332\345\360\355\361\363\370\377\5*\213\377\13/\216\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14/\217\377\14/\216\377\14""0\217\377\14""0"
  "\217\377\14""1\220\377\14""1\217\377\14""1\220\377\14""2\220\377\14""2\221"
  "\377\14""3\221\377\14""3\221\377\14""3\221\377\14""4\222\377\14""5\223\377"
  "\13""5\223\377\14""5\223\377\13""6\223\377\7""3\222\377e\201\274\377\377"
  "\377\377\357\376\376\376b\0\0\0\0\346\355\365o\377\377\377\362d\201\274\377"
  "\14;\227\377\12:\226\377\13:\227\377\13;\230\377\13;\227\377\13<\230\377"
  "\13<\230\377\12=\231\377\13=\231\377\13>\231\377\12>\231\377\12>\232\377"
  "\11?\232\377\12?\232\377\12@\233\377\12@\233\377\12@\233\377\12A\234\377"
  "\12A\234\377\12B\234\377\12B\235\377\12B\235\377\12C\235\377\12D\235\377"
  "\12D\236\377\11D\237\377\12E\237\377\12E\237\377\11F\240\377\11F\240\377"
  "\10F\240\377\11G\241\377\11G\241\377\11H\241\377\11H\241\377\11H\242\377"
  "\11I\242\377\4F\241\377X\204\300\377\373\374\375\377\327\343\357\377\352"
  "\355\364\376\0$\211\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14/\216\377\14""0\217\377\14""0\217"
  "\377\14""1\217\377\14""1\217\377\14""1\220\377\14""2\220\377\14""2\220\377"
  "\14""3\221\377\14""3\222\377\14""4\222\377\14""4\222\377\14""5\222\377\14"
  """5\223\377\13""5\222\377\10""3\222\377f\200\273\377\377\377\377\357\376"
  "\376\376b\0\0\0\0\346\355\365o\377\377\377\362d\201\274\377\14:\226\377\12"
  """9\226\377\13:\226\377\13:\227\377\13;\227\377\13;\230\377\13<\227\377\13"
  "<\230\377\13=\231\377\12=\231\377\13>\231\377\12>\232\377\12>\232\377\11"
  "?\232\377\11?\232\377\12@\233\377\12@\233\377\12@\233\377\12A\234\377\12"
  "A\234\377\12A\234\377\12B\234\377\12C\235\377\12C\236\377\12D\235\377\12"
  "D\236\377\12D\237\377\11E\237\377\12E\237\377\11F\240\377\11F\240\377\10"
  "F\240\377\10G\240\377\11G\240\377\11G\241\377\11H\241\377\11H\241\377\4F"
  "\240\377U\201\277\377\373\374\375\377\334\346\361\377\346\351\363\377\0%"
  "\211\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\13/\216\377\13/"
  "\216\377\13/\216\377\13/\217\377\14""0\217\377\13/\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14/\216\377\14/\216\377\14""0\217\377\14""0"
  "\217\377\14""1\220\377\14""1\220\377\14""2\220\377\14""2\220\377\14""2\221"
  "\377\14""3\221\377\14""4\222\377\14""4\221\377\14""4\222\377\13""4\222\377"
  "\10""2\221\377f\200\273\377\377\377\377\357\376\376\376b\0\0\0\0\346\355"
  "\365o\377\377\377\362d\201\273\377\14""9\226\377\12""8\225\377\13""9\226"
  "\377\13:\226\377\13;\227\377\13;\227\377\13;\230\377\13<\230\377\13<\230"
  "\377\13<\230\377\12=\231\377\13=\231\377\12>\232\377\12>\232\377\10<\231"
  "\377\5;\230\377\5<\231\377\5<\231\377\5=\231\377\5=\232\377\7?\233\377\11"
  "A\234\377\12B\235\377\12C\235\377\12C\236\377\12C\236\377\12D\236\377\12"
  "D\236\377\11D\237\377\12E\237\377\11E\237\377\11F\237\377\11F\240\377\10"
  "G\240\377\11G\241\377\11G\241\377\11H\241\377\4E\240\377U\201\277\377\373"
  "\374\375\377\334\346\362\377\346\352\363\377\0%\211\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\5*\213\377\0$\210\377\0$\210\377\0$\210\377\0#\210\377"
  "\0#\210\377\2'\212\377\11-\215\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14/\216\377\14""0\217\377\14""0\217\377\14""1\217\377"
  "\14""1\217\377\14""2\220\377\14""2\221\377\14""3\221\377\14""3\221\377\14"
  """3\222\377\14""4\221\377\13""4\222\377\10""2\221\377f\200\272\377\377\377"
  "\377\357\376\376\376b\0\0\0\0\346\355\365o\377\377\377\362d\201\273\377\14"
  """9\225\377\12""8\225\377\13""9\226\377\13""9\226\377\13:\226\377\13:\227"
  "\377\13;\227\377\13;\227\377\13;\230\377\13<\230\377\13<\230\377\12=\231"
  "\377\13=\231\377\11<\230\377\31J\240\377*X\247\377*X\250\377*Y\250\377*Y"
  "\250\377'X\250\377\22F\237\377\0""7\227\377\3<\231\377\12B\235\377\12B\235"
  "\377\12C\235\377\12C\236\377\12D\236\377\12D\236\377\11D\237\377\12E\237"
  "\377\12E\237\377\11F\240\377\11F\240\377\10G\240\377\10G\240\377\11G\240"
  "\377\4E\237\377U\200\276\377\373\374\375\377\334\346\362\377\346\352\363"
  "\377\0%\211\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\10-\215\377Mg\255\377\241\257\323\377"
  "\241\257\324\377\241\257\324\377\237\255\323\377\222\242\314\376d{\267\377"
  "\34?\226\377\1&\211\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14/\216\377\14""0\216\377\14""0\217\377\14""1\217\377\14""1\217\377"
  "\14""2\220\377\14""2\220\377\14""2\221\377\14""3\221\377\14""3\222\377\13"
  """3\221\377\10""1\220\377f\177\272\377\377\377\377\357\376\376\376b\0\0\0"
  "\0\346\355\365o\377\377\377\362d\200\273\377\14""8\225\377\12""7\225\377"
  "\13""8\225\377\13""9\226\377\13""9\225\377\13:\226\377\13:\227\377\13;\227"
  "\377\13;\227\377\13;\230\377\13<\230\377\13<\230\377\13=\231\377\0""5\224"
  "\377f\206\277\377\317\332\352\376\331\341\356\377\331\341\356\377\331\341"
  "\356\377\327\340\356\377\320\332\354\377\247\273\333\377=i\260\377\0""9\230"
  "\377\12B\235\377\12B\234\377\12C\235\377\12C\236\377\12C\236\377\12D\236"
  "\377\12D\236\377\11E\237\377\12E\237\377\11F\240\377\11F\237\377\11F\240"
  "\377\10G\240\377\4C\237\377U\200\276\377\373\374\375\377\334\346\362\377"
  "\346\352\363\377\0%\211\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\6+\214\377t\211\277\377"
  "\372\372\374\376\375\375\375\377\372\373\374\377\373\373\374\377\371\372"
  "\373\377\357\361\367\377\313\323\346\377Vo\261\376\1&\212\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14/\217\377\14/\216\377\14""0"
  "\217\377\14""0\220\377\14""1\220\377\14""2\217\377\14""1\220\377\14""2\221"
  "\377\14""3\221\377\13""3\221\377\10""0\220\377f\177\272\377\377\377\377\357"
  "\376\376\376b\0\0\0\0\346\355\365o\377\377\377\362d\200\272\377\14""8\224"
  "\377\12""7\224\377\13""7\224\377\13""8\225\377\13""9\226\377\13""9\225\377"
  "\13:\226\377\13:\226\377\13:\227\377\13;\227\377\13;\230\377\13<\230\377"
  "\13<\230\377\0""2\223\377{\225\307\377\375\375\376\377\345\355\365\342\323"
  "\341\356\325\324\341\357\326\333\345\361\325\350\356\365\360\373\374\374"
  "\377\343\351\363\376U{\272\377\3<\231\377\12B\235\377\12B\235\377\12C\235"
  "\377\12C\235\377\12C\236\377\12D\236\377\12D\236\377\11D\237\377\12E\237"
  "\377\12E\237\377\11F\240\377\11F\240\377\4C\237\377U\200\276\377\373\374"
  "\375\377\334\346\362\377\346\352\363\377\0%\211\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\6"
  "+\214\377v\212\300\377\376\376\376\377\277\323\347\211\377\377\377\32\311"
  "\331\352.\231\270\331b\346\356\365\233\364\367\372\377\326\333\353\377Jd"
  "\253\376\2(\212\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14/\216\377\14""0\217\377\14""1\217\377\14""1\220\377\14""2"
  "\217\377\14""2\220\377\14""2\221\377\13""2\220\377\10/\217\377f~\272\377"
  "\377\377\377\357\376\376\376b\0\0\0\0\346\355\365o\377\377\377\362d\177\272"
  "\377\15""7\224\377\12""6\223\377\12""7\224\377\12""8\224\377\13""8\225\377"
  "\13""9\226\377\13""9\225\377\13:\225\377\13:\226\377\13:\227\377\13;\227"
  "\377\13;\230\377\13;\230\377\0""2\222\377y\224\306\377\367\371\373\377\365"
  "\370\372\\\237\274\334\20\273\320\345\26\304\325\350\26\337\347\361\27\277"
  "\322\346e\361\365\371\377\343\351\363\376\32M\242\377\10?\233\377\12A\234"
  "\377\12B\235\377\12B\234\377\12C\235\377\12C\236\377\12D\235\377\12C\236"
  "\377\12D\237\377\11E\237\377\12E\237\377\11F\240\377\4C\236\377U\200\276"
  "\377\373\374\375\377\334\346\362\377\346\352\363\377\0%\211\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\6+\214\377v\212\300\377\375\375\375\377\252\303\340x\0\0\0\0"
  "\0\0\0\0)j\260\3\262\311\341\7\342\353\364d\371\374\375\377\241\256\323\377"
  "\17""2\220\377\12.\216\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14/\216\377\14""0\216\377\14""0\217\377\14""1\220"
  "\377\14""1\217\377\14""2\220\377\13""2\220\377\10/\217\377f~\271\377\377"
  "\377\377\357\375\375\375b\0\0\0\0\346\355\365o\377\377\377\362d~\271\377"
  "\15""6\224\377\13""6\223\377\13""7\224\377\12""7\225\377\12""8\224\377\13"
  """8\225\377\13""9\226\377\13""9\226\377\13""9\225\377\13:\226\377\13:\227"
  "\377\13;\227\377\13:\227\377\0""1\222\377y\224\306\377\367\371\373\377\377"
  "\377\377K\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\14S\243\1\313\333\353\220\373\373"
  "\374\377z\227\310\377\2;\230\377\12@\233\377\12A\234\377\12B\235\377\12B"
  "\234\377\12C\235\377\12C\236\377\12D\235\377\12C\236\377\12D\237\377\11E"
  "\236\377\12E\237\377\5B\236\377U\177\276\377\373\374\375\377\334\346\362"
  "\377\346\352\363\377\0%\211\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\6+\214\377v\212\300"
  "\377\375\375\375\377\253\304\340y\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\267\314"
  "\344$\351\360\367\276\321\330\351\377>Z\246\377\5*\214\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  "/\217\377\14/\216\377\14""0\217\377\14""1\220\377\14""1\220\377\13""1\217"
  "\377\10.\216\377f}\271\377\377\377\377\357\375\375\375b\0\0\0\0\346\355\365"
  "o\377\377\377\362d~\272\377\15""5\223\377\13""5\222\377\14""5\223\377\14"
  """6\223\377\12""7\225\377\13""8\225\377\13""8\225\377\13""8\225\377\13""9"
  "\226\377\13""9\225\377\13:\226\377\13:\227\377\13;\226\377\0""1\222\377y"
  "\223\306\377\367\370\373\377\377\377\376L\0\0\0\0\0\0\0\0\0\0\0\0\250\302"
  "\335\0\0\0\0\0m\232\310'\372\373\375\377\300\316\344\377\0""6\226\377\12"
  "A\234\377\12@\233\377\12A\234\377\12B\235\377\12B\234\377\12C\235\377\12"
  "B\236\377\12C\236\377\12D\235\377\12C\236\377\11D\237\377\5A\235\377U\177"
  "\276\377\373\374\375\377\334\346\362\377\346\352\363\377\0%\211\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\6+\214\377v\212\300\377\375\375\375\377\253\304\340y\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\37e\255\13\334\347\362\212\354\356\365\377]u\264"
  "\377\2(\212\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14/\216\377\14""0\216"
  "\377\14""1\217\377\13""0\217\377\10/\216\377f~\270\377\377\377\377\357\376"
  "\376\376b\0\0\0\0\346\355\365o\377\377\377\362d~\272\377\15""5\223\377\13"
  """4\222\377\14""5\222\377\13""5\223\377\14""6\223\377\12""7\224\377\13""8"
  "\225\377\12""7\224\377\13""8\225\377\13""9\226\377\13""9\225\377\13:\225"
  "\377\13""9\226\377\0""0\221\377y\223\305\377\367\370\372\377\377\377\377"
  "L\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0a\221\303\0\366\370\373\0\325\342\357\377"
  "\351\355\365\377\0""6\226\377\11@\233\377\12A\234\377\12@\233\377\12@\233"
  "\377\12A\234\377\12A\235\377\12B\234\377\12B\235\377\12C\236\377\12D\235"
  "\377\12C\236\377\5A\235\377U\177\275\377\373\374\375\377\334\346\362\377"
  "\346\352\363\377\0%\211\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\6+\214\377v\212\300\377"
  "\375\375\375\377\253\304\340y\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\365"
  "\371\372`\365\367\372\377n\203\274\377\1&\211\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14/\217\377\14/\216\377\13/\217\377\10-\216"
  "\377f}\270\377\377\377\377\357\376\376\376b\0\0\0\0\346\355\365o\377\377"
  "\377\362d~\272\377\15""4\222\377\13""4\222\377\14""4\223\377\14""5\222\377"
  "\13""5\223\377\14""6\223\377\13""6\224\377\12""7\225\377\12""8\224\377\12"
  """7\224\377\13""8\225\377\13""8\226\377\13""9\225\377\0""0\221\377y\223\305"
  "\377\367\370\372\377\377\377\377L\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\364\366\372\0\365\367\372\312\354\360\367\377\14A\233\377\10>\232\377\12"
  "?\232\377\12@\233\377\12A\234\377\12@\233\377\12A\234\377\12A\235\377\12"
  "B\234\377\12B\235\377\12B\235\377\12C\235\377\5@\234\377U~\274\377\373\374"
  "\375\377\334\346\362\377\346\352\363\377\0%\211\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\6"
  "+\214\377v\212\300\377\375\375\375\377\253\304\340y\0\0\0\0\0\0\0\0\0\0\0"
  "\0*k\261\0\0\0\0\0\354\363\370]\362\366\372\377u\211\277\377\0&\211\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\13/\216\377\10,\215\377f}\271\377\377\377\377\357\376\376\376b\0\0\0"
  "\0\346\355\365o\377\377\377\362d}\271\377\15""4\222\377\13""3\221\377\14"
  """4\222\377\14""4\222\377\14""4\222\377\13""5\222\377\14""6\223\377\13""6"
  "\223\377\13""7\224\377\13""7\225\377\12""7\224\377\13""8\225\377\13""8\226"
  "\377\0/\220\377y\223\305\377\367\370\372\377\377\377\377L\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\360\364\370\0\362\365\371\307\355\360\367\377\24"
  "E\235\377\10=\231\377\11?\232\377\11?\232\377\12@\233\377\12@\233\377\12"
  "@\233\377\12A\234\377\12A\234\377\12A\234\377\12B\234\377\12B\235\377\5@"
  "\234\377U~\273\377\373\374\375\377\334\346\362\377\346\352\363\377\0%\211"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\6+\214\377v\212\300\377\375\375\375\377\253\304"
  "\340y\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\352\361\367]\362\366\372\377"
  "v\212\277\377\0&\211\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\13/\216\377\10,\215\377f|\271\377\377\377"
  "\377\357\376\376\376b\0\0\0\0\346\355\365o\377\377\377\362d}\271\377\15""3"
  "\222\377\13""3\221\377\14""3\221\377\14""4\221\377\14""4\222\377\14""4\223"
  "\377\13""5\222\377\14""5\223\377\14""6\223\377\13""6\224\377\13""7\224\377"
  "\12""7\224\377\13""8\225\377\0.\220\377y\222\305\377\367\370\372\377\377"
  "\377\377L\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\360\364\370\0\362\365\371"
  "\307\355\360\367\377\24D\235\377\11<\231\377\12>\231\377\12>\232\377\11?"
  "\232\377\12?\232\377\12@\233\377\12@\233\377\12@\233\377\12A\234\377\12A"
  "\235\377\12B\234\377\5?\233\377U}\273\377\373\374\375\377\334\346\362\377"
  "\346\352\363\377\0%\211\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\6+\214\377v\212\300\377"
  "\375\375\375\377\253\304\340y\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\352"
  "\362\367]\362\366\372\377v\212\277\377\0&\211\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\13/\216\377\10,"
  "\215\377f}\271\377\377\377\377\357\376\376\376b\0\0\0\0\346\355\365o\377"
  "\377\377\362d|\271\377\15""3\221\377\13""2\220\377\14""3\221\377\14""3\222"
  "\377\14""3\221\377\14""4\222\377\14""4\223\377\14""5\222\377\13""5\223\377"
  "\14""6\223\377\13""6\224\377\13""7\224\377\12""7\224\377\0-\217\377y\222"
  "\304\377\367\370\372\377\377\377\377L\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\360\364\370\0\362\365\371\307\355\360\367\377\24D\235\377\11<\231\377"
  "\13=\231\377\12>\231\377\12>\232\377\11?\232\377\11?\232\377\12@\233\377"
  "\12@\233\377\12@\233\377\12A\234\377\12A\234\377\5?\233\377U}\273\377\373"
  "\374\375\377\334\346\362\377\346\352\363\377\0%\211\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\6+\214\377v\212\300\377\375\375\375\377\253\304\340y\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\352\362\367]\362\366\372\377v\212\277\377\0&\211"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\13/\216\377\10,\215\377f}\271\377\377\377\377\357\376\376\376"
  "b\0\0\0\0\346\355\365o\377\377\377\362d|\270\377\15""2\221\377\13""2\220"
  "\377\14""2\220\377\14""2\221\377\14""3\222\377\14""3\221\377\14""4\222\377"
  "\14""4\222\377\14""5\223\377\13""5\223\377\14""6\223\377\13""6\223\377\13"
  """7\224\377\0-\217\377y\221\304\377\367\370\372\377\377\377\377L\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\360\364\370\0\362\365\371\307\355\360\367"
  "\377\24C\234\377\11;\230\377\12=\231\377\13=\231\377\12>\231\377\12>\232"
  "\377\12?\232\377\11?\232\377\12@\233\377\12@\233\377\12@\233\377\12@\233"
  "\377\5>\232\377U|\273\377\373\374\375\377\334\346\362\377\346\352\363\377"
  "\0%\211\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\6+\214\377v\212\300\377\375\375\375\377"
  "\253\304\340y\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\352\362\367]\362\366"
  "\372\377v\212\277\377\0&\211\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\13/\216\377\10,\215\377f}\271\377"
  "\377\377\377\357\376\376\376b\0\0\0\0\346\355\365o\377\377\377\362d|\270"
  "\377\15""2\220\377\13""1\217\377\14""1\220\377\14""2\220\377\14""2\221\377"
  "\14""3\221\377\14""3\221\377\14""4\221\377\14""4\222\377\14""5\223\377\13"
  """5\223\377\14""5\223\377\14""6\223\377\0,\216\377y\221\304\377\367\370\372"
  "\377\377\377\377L\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\360\364\370\0\362"
  "\365\371\307\355\360\367\377\24C\234\377\11;\227\377\13<\230\377\12=\231"
  "\377\13=\231\377\13>\231\377\12>\231\377\12>\232\377\11?\232\377\12?\232"
  "\377\12@\233\377\12A\234\377\5=\232\377U|\272\377\373\374\375\377\334\346"
  "\362\377\346\352\363\377\0%\211\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\6+\214\377v\212"
  "\300\377\375\375\375\377\253\304\340y\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\352\362\367]\362\366\372\377v\212\277\377\0&\211\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\13/\216"
  "\377\10,\215\377f}\271\377\377\377\377\357\376\376\376b\0\0\0\0\346\355\365"
  "o\377\377\377\362d{\267\377\15""1\217\377\13""1\217\377\14""2\217\377\14"
  """1\220\377\14""2\220\377\14""3\220\377\14""3\221\377\14""3\222\377\14""4"
  "\222\377\14""4\222\377\14""4\223\377\14""5\222\377\13""5\223\377\0,\216\377"
  "z\221\304\377\367\370\373\377\377\377\377L\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\360\364\370\0\362\365\371\307\355\360\367\377\24B\233\377\11:"
  "\227\377\13<\230\377\13<\230\377\13=\230\377\12=\231\377\13=\231\377\12>"
  "\231\377\12>\232\377\11?\232\377\11?\232\377\12@\233\377\5=\232\377U|\272"
  "\377\373\374\375\377\334\346\362\377\346\352\363\377\0%\211\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\6+\214\377v\212\300\377\375\375\375\377\253\304\340y\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\352\362\367]\362\366\372\377v\212\277\377"
  "\0&\211\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\13/\216\377\10,\215\377f}\271\377\377\377\377\357\376"
  "\376\376b\0\0\0\0\346\355\365o\377\377\377\362d{\267\377\15""1\217\377\13"
  """0\217\377\14""1\220\377\14""1\217\377\14""1\220\377\14""2\221\377\14""3"
  "\221\377\14""3\221\377\14""3\222\377\14""4\222\377\14""4\222\377\14""5\222"
  "\377\14""5\223\377\0+\215\377z\220\304\377\367\370\373\377\377\377\377L\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\360\364\370\0\362\365\371\307\355"
  "\360\367\377\24A\233\377\11:\226\377\13;\230\377\13<\230\377\13<\230\377"
  "\13=\231\377\12=\231\377\13>\231\377\12>\231\377\12>\232\377\12?\232\377"
  "\11?\232\377\5<\231\377U{\272\377\373\374\375\377\334\346\362\377\346\352"
  "\363\377\0%\211\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\6+\214\377v\212\300\377\375\375\375"
  "\377\253\304\340y\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\352\362\367]\362"
  "\366\372\377v\212\277\377\0&\211\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\13/\216\377\10,\215\377f}\271"
  "\377\377\377\377\357\376\376\376b\0\0\0\0\346\355\365o\377\377\377\362d{"
  "\267\377\15""1\217\377\13/\216\377\14""0\217\377\14""0\220\377\14""1\220"
  "\377\14""2\220\377\14""2\220\377\14""2\221\377\14""3\221\377\14""3\221\377"
  "\14""4\222\377\14""4\222\377\14""4\222\377\0+\215\377z\220\304\377\367\370"
  "\373\377\377\377\377L\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\360\364\370"
  "\0\362\365\371\307\355\360\366\377\24B\232\377\11""9\226\377\13;\227\377"
  "\13;\230\377\13<\230\377\13<\230\377\13<\230\377\12=\231\377\13=\231\377"
  "\13>\232\377\12>\231\377\12>\232\377\5;\231\377U{\271\377\373\374\375\377"
  "\334\346\362\377\346\352\363\377\0%\211\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\6+\214\377"
  "v\212\300\377\375\375\375\377\253\304\340y\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\352\362\367]\362\366\372\377v\212\277\377\0&\211\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\13"
  "/\216\377\10,\215\377f}\271\377\377\377\377\357\376\376\376b\0\0\0\0\346"
  "\355\365o\377\377\377\362d{\267\377\15""1\217\377\13/\216\377\14/\216\377"
  "\14""0\217\377\14""1\217\377\14""1\220\377\14""2\217\377\14""2\220\377\14"
  """2\221\377\14""3\221\377\14""3\221\377\14""3\222\377\14""4\222\377\0*\214"
  "\377z\220\303\377\367\370\373\377\377\377\377L\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\360\364\370\0\362\365\371\307\355\360\366\377\24A\232\377"
  "\11""9\225\377\13:\227\377\13;\227\377\13;\227\377\13;\230\377\13<\230\377"
  "\13<\230\377\13=\231\377\12=\231\377\13>\231\377\12>\231\377\5;\230\377U"
  "z\271\377\373\374\375\377\334\346\362\377\346\352\363\377\0%\211\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\6+\214\377v\212\300\377\375\375\375\377\253\304\340y\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\352\362\367]\362\366\372\377v\212\277"
  "\377\0&\211\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\13/\216\377\10,\215\377f}\271\377\377\377\377\357"
  "\376\376\376b\0\0\0\0\346\355\365o\377\377\377\362d{\267\377\15""1\217\377"
  "\13/\216\377\14""0\217\377\14/\216\377\14""0\216\377\14""0\217\377\14""1"
  "\220\377\14""1\220\377\14""2\220\377\14""2\220\377\14""2\221\377\14""3\221"
  "\377\14""3\222\377\0)\214\377z\220\303\377\367\370\372\377\377\377\377L\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\360\364\370\0\362\365\371\307\355"
  "\360\366\377\24@\232\377\11""7\225\377\13:\226\377\13:\227\377\13;\227\377"
  "\13;\227\377\13;\230\377\13<\230\377\13<\230\377\13=\231\377\12=\231\377"
  "\13=\231\377\6:\230\377Uy\271\377\373\374\375\377\334\346\362\377\346\352"
  "\363\377\0%\211\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\6+\214\377v\212\300\377\375\375\375"
  "\377\253\304\340y\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\352\362\367]\362"
  "\366\372\377v\212\277\377\0&\211\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\13/\216\377\10,\215\377f}\271"
  "\377\377\377\377\357\376\376\376b\0\0\0\0\346\355\365o\377\377\377\362d{"
  "\267\377\15""1\217\377\13/\216\377\14""0\217\377\14""0\217\377\14/\217\377"
  "\14/\216\377\14""0\217\377\14""1\217\377\14""1\220\377\14""2\220\377\14""2"
  "\220\377\14""2\221\377\14""3\221\377\0(\214\377z\217\303\377\367\370\373"
  "\377\377\377\377L\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\360\364\370\0\362"
  "\365\371\307\355\360\366\377\24@\231\377\11""7\225\377\13""9\226\377\13:"
  "\226\377\13:\226\377\13:\227\377\13;\227\377\13;\230\377\13<\230\377\13<"
  "\230\377\13<\230\377\12=\231\377\6:\227\377Vz\271\377\373\374\375\377\334"
  "\346\362\377\346\352\363\377\0%\211\377\14""0\217\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217"
  "\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\6+\214\377v"
  "\212\300\377\375\375\375\377\253\304\340y\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\352\362\367]\362\366\372\377v\212\277\377\0&\211\377\14""0\217\377"
  "\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14"
  """0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\14""0\217\377\13/\216"
  "\377\10-\215\377f}\271\377\377\377\377\357\376\376\376b\0\0\0\0\346\355\365"
  "o\377\377\377\362d{\267\377\15""1\217\377\13/\216\377\14""0\217\377\14""0"
  "\217\377\14""0\217\377\14""0\216\377\14""0\216\377\14""0\216\377\14""0\217"
  "\377\14""1\220\377\14""1\220\377\14""2\220\377\14""2\221\377\0(\213\377z"
  "\217\302\377\367\370\372\377\377\377\377L\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\360\364\370\0\362\365\371\307\355\360\366\377\24?\231\377\11""6"
  "\224\377\13""9\226\377\13""9\226\377\13""9\226\377\13:\226\377\13:\227\377"
  "\13;\227\377\13;\227\377\13;\230\377\13<\230\377\13<\230\377\6""9\227\377"
  "Vy\271\377\373\374\375\377\334\346\362\377\345\351\363\376\0\37\205\377\4"
  "*\213\377\4*\213\377\5*\213\377\5*\213\377\5*\213\377\5*\213\377\5*\213\377"
  "\5*\213\377\5*\214\377\5*\214\377\5*\214\377\5*\214\377\0&\211\377s\207\276"
  "\377\375\375\375\377\253\304\340y\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\353\362\367]\363\367\372\377o\204\274\376\0\33\204\377\0&\212\377\0&\212"
  "\377\0&\212\377\0&\212\377\0&\212\377\0&\212\377\0&\212\377\0&\212\377\0"
  "&\212\377\0&\212\377\0&\212\377\0%\212\377\0\"\210\377^v\265\377\377\377"
  "\377\357\376\376\376b\0\0\0\0\347\356\366o\377\377\377\362]u\264\377\1'\212"
  "\377\0%\212\377\0&\212\377\0&\212\377\0&\212\377\0&\212\377\0&\212\377\0"
  "&\212\377\0&\212\377\0&\212\377\0'\212\377\0'\213\377\0(\213\377\0\35\205"
  "\377s\211\277\377\367\370\372\377\377\377\377L\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\361\365\371\0\362\366\371\307\354\357\366\377\10""5\223\377"
  "\0-\217\377\0.\220\377\0.\220\377\0/\220\377\0""0\221\377\0""0\221\377\0"
  """0\222\377\0""1\221\377\0""1\222\377\0""2\223\377\0""2\223\377\0/\220\377"
  "Nr\264\377\373\374\374\377\330\343\360\377\363\364\371\376~\221\303\377\203"
  "\224\305\377\202\224\305\377\201\223\304\377\200\222\304\377~\221\303\377"
  "|\220\302\377{\216\301\377x\214\301\377w\213\300\377u\211\277\377s\207\276"
  "\377q\205\275\377k\201\272\377\254\271\331\377\376\376\376\377\250\302\336"
  "z\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\335\347\362^\355\362\370\377\334"
  "\341\356\376\274\305\337\376\277\310\341\377\277\310\341\377\277\310\341"
  "\377\277\310\341\377\277\310\341\377\277\310\341\377\277\310\341\377\277"
  "\310\341\377\277\310\341\377\277\310\341\377\277\310\341\377\277\310\341"
  "\377\276\307\340\376\326\335\353\377\376\376\376\361\356\363\370d\0\0\0\0"
  "\327\343\360q\371\373\374\363\327\334\354\377\277\310\341\376\277\310\341"
  "\377\277\310\341\377\277\310\341\377\277\310\341\377\277\310\341\377\277"
  "\310\341\377\277\310\341\377\277\310\341\377\277\310\341\377\277\310\341"
  "\377\277\311\342\377\277\311\342\377\274\306\340\376\334\342\357\376\363"
  "\366\371\377\360\364\370M\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\342\353"
  "\364\0\344\354\365\311\373\373\374\377\301\314\343\376\276\312\343\377\277"
  "\312\343\377\277\312\343\377\277\312\343\377\277\312\343\377\277\312\343"
  "\377\277\312\343\377\277\313\343\377\277\313\343\377\277\313\343\377\277"
  "\313\343\377\276\312\343\377\322\333\354\376\376\376\376\377\335\347\362"
  "\220\376\376\376\377\374\374\375\377\374\374\375\377\374\374\375\377\374"
  "\374\375\377\374\374\375\377\374\374\375\377\374\374\375\377\374\374\375"
  "\377\374\374\375\377\374\374\375\377\374\374\375\377\373\374\375\377\373"
  "\374\375\377\373\374\375\377\375\375\375\377\357\364\371\377~\246\320*\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\231\266\330/\323\340\356\311\373\373"
  "\375\377\374\374\375\377\374\374\375\377\374\374\375\377\374\374\375\377"
  "\374\374\375\377\374\374\375\377\374\374\375\377\374\374\375\377\374\374"
  "\375\377\374\374\375\377\374\374\375\377\374\374\375\377\374\374\375\377"
  "\374\374\375\377\367\371\374\377\335\347\362\303\222\263\325(\0\0\0\0\205"
  "\251\321#\333\346\361\310\367\371\373\377\374\374\375\377\374\374\375\377"
  "\374\374\375\377\374\374\375\377\374\374\375\377\374\374\375\377\374\374"
  "\375\377\374\374\375\377\374\374\375\377\374\374\375\377\374\374\375\377"
  "\374\374\375\377\374\374\375\377\374\374\375\377\374\375\375\377\321\337"
  "\356\314\243\276\335+\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\255\306\340"
  "\0\265\313\342X\353\360\367\377\375\375\376\377\374\374\375\377\374\374\375"
  "\377\374\374\375\377\374\374\375\377\374\374\375\377\374\374\375\377\374"
  "\374\375\377\374\374\375\377\374\374\375\377\374\374\375\377\374\374\375"
  "\377\374\374\375\377\374\375\376\377\335\347\362\355",
};

static void exynos4412_display_logo(void)
{
	struct surface_t * screen = exynos4412_screen_surface();
	struct surface_t * logo;
	struct rect_t rect;
	u32_t c;

	exynos4412_screen_flush();
	c = surface_map_color(screen, get_named_color("black"));
	surface_fill(screen, &screen->clip, c, BLEND_MODE_REPLACE);

	logo = surface_alloc_from_gimage(&default_logo);
	if(!logo)
		return;
	surface_set_clip_rect(screen, NULL);
	surface_set_clip_rect(logo, NULL);
	rect_align(&(screen->clip), &(logo->clip), &rect, ALIGN_CENTER);
	if (!rect_intersect(&(screen->clip), &rect, &rect))
	{
		surface_free(logo);
		return;
	}

	surface_fill(screen, &screen->clip, c, BLEND_MODE_REPLACE);
	surface_blit(screen, &rect, logo, &(logo->clip), BLEND_MODE_ALPHA);
	surface_free(logo);
}

void exynos4412_fb_initial(char * commandline)
{
	struct exynos4412_fb_data_t * dat;

	if(strstr(commandline, "lcd=vga-1024x768"))
		dat = &vga_1024_768;
	else if(strstr(commandline, "lcd=vga-1440x900"))
		dat = &vga_1440_900;
	else if(strstr(commandline, "lcd=vga-1280x1024"))
		dat = &vga_1280_1024;
	else if(strstr(commandline, "lcd=ek070tn93"))
		dat = &ek070tn93;
	else
		dat = &vs070cxn;
		
	if( (dat->bits_per_pixel != 16) && (dat->bits_per_pixel != 24) && (dat->bits_per_pixel != 32) )
		return;

	exynos4412_fb.dat = dat;
	exynos4412_fb.surface.info.bits_per_pixel = dat->bits_per_pixel;
	exynos4412_fb.surface.info.bytes_per_pixel = dat->bytes_per_pixel;
	exynos4412_fb.surface.info.red_mask_size = dat->rgba.r_mask;
	exynos4412_fb.surface.info.red_field_pos = dat->rgba.r_field;
	exynos4412_fb.surface.info.green_mask_size = dat->rgba.g_mask;
	exynos4412_fb.surface.info.green_field_pos = dat->rgba.g_field;
	exynos4412_fb.surface.info.blue_mask_size = dat->rgba.b_mask;
	exynos4412_fb.surface.info.blue_field_pos = dat->rgba.b_field;
	exynos4412_fb.surface.info.alpha_mask_size = dat->rgba.a_mask;
	exynos4412_fb.surface.info.alpha_field_pos = dat->rgba.a_field;
	exynos4412_fb.surface.info.fmt = get_pixel_format(&(exynos4412_fb.surface.info));

	exynos4412_fb.surface.w = dat->width;
	exynos4412_fb.surface.h = dat->height;
	exynos4412_fb.surface.pitch = dat->width * dat->bytes_per_pixel;
	exynos4412_fb.surface.flag = SURFACE_PIXELS_DONTFREE;
	exynos4412_fb.surface.pixels = dat->vram_front;

	exynos4412_fb.surface.clip.x = 0;
	exynos4412_fb.surface.clip.y = 0;
	exynos4412_fb.surface.clip.w = dat->width;
	exynos4412_fb.surface.clip.h = dat->height;

	memset(&exynos4412_fb.surface.maps, 0, sizeof(struct surface_maps));
	surface_set_maps(&exynos4412_fb.surface.maps);

	fb_init(&exynos4412_fb);

	exynos4412_display_logo();
	if(dat->backlight)
		dat->backlight(255);
	
}

struct surface_t * exynos4412_screen_surface(void)
{
	return &exynos4412_fb.surface;
}

void exynos4412_screen_swap(void)
{
	fb_swap(&exynos4412_fb);
}

void exynos4412_screen_flush(void)
{
	fb_flush(&exynos4412_fb);
}

void exynos4412_screen_backlight(u8_t brightness)
{
	fb_backlight(&exynos4412_fb, brightness);
}

void exynos4412_set_progress(int percent)
{
	static int history = -1;
	struct surface_t * screen = exynos4412_screen_surface();
	struct surface_t * obj;
	struct rect_t rect;
	u32_t fc, bc;
	s32_t x, y, w, h;
	s32_t i;

	if(history == percent)
		return;

	x = 40;
	y = screen->h - 50;
	w = screen->w - 120;
	h = 8 + 10;

	fc = surface_map_color(screen, get_named_color("white"));
	bc = surface_map_color(screen, get_named_color("black"));

	obj = surface_alloc(NULL, w, h, PIXEL_FORMAT_ABGR_8888);
	if(!obj)
		return;
	surface_fill(obj, NULL, bc, BLEND_MODE_REPLACE);
	for(i = 0; i < h; i++)
	{
		surface_point(obj, 0, i, fc, BLEND_MODE_REPLACE);
		surface_point(obj, w - 1, i, fc, BLEND_MODE_REPLACE);
	}
	for(i = 0; i < w; i++)
	{
		surface_point(obj, i, 0, fc, BLEND_MODE_REPLACE);
		surface_point(obj, i, h - 1, fc, BLEND_MODE_REPLACE);
	}
	rect.x = 0 + 2;
	rect.y = 0 + 3;
	rect.w = (w - 4) * percent / 100;
	rect.h = h - 6;
	surface_fill(obj, &rect, fc, BLEND_MODE_REPLACE);
	
	rect.x = x;
	rect.y = y;
	surface_blit(screen, &rect, obj, NULL, BLEND_MODE_REPLACE);
	surface_free(obj);
	
	lcd_print(x + w + 8, y + 2, fc, bc, "%3d%%", percent);
}

void exynos4412_set_messge(const char * fmt, ...)
{
	struct surface_t * screen = exynos4412_screen_surface();
	u32_t fc, bc;
	va_list ap;
	char buf[1024];
	int len;
	s32_t x, y, w, h;
	
	x = 40;
	y = screen->h - 50 - 18;
	w = screen->w - 120;
	h = 8 + 10;
	fc = surface_map_color(screen, get_named_color("white"));
	bc = surface_map_color(screen, get_named_color("black"));

	memset(buf, ' ', sizeof(buf));
	va_start(ap, fmt);
	len = vsprintf(buf, fmt, ap);
	va_end(ap);

	len = w / 8;
	lcd_textout(x, y, fc, bc, (u8_t *)buf, len);
}
