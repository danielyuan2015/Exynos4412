/* include/linux/kxud9.h - KXUD9 accelerometer driver
 *
 * Copyright (C) 2010 Kionix, Inc.
 * Written by Kuching Tan <kuchingtan@kionix.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __KXUD9_H__
#define __KXUD9_H__

//#define KXUD9_I2C_ADDR	0x18	// ADDR pin = 0
#define KXUD9_I2C_ADDR	0x19	// ADDR pin = 1
/* CTRL_REGC BITS */
#define KXUD9_G_8G 		0x00
#define KXUD9_G_6G 		0x01
#define KXUD9_G_4G 		0x02
#define KXUD9_G_2G 		0x03
#define MOTLAT			0x08
#define MOTLEV			0x10
#define ODR2000D		0x60
#define ODR1000D		0x80
#define ODR500D			0xA0
#define ODR100D			0xC0
#define ODR50D			0xE0
/* CTRL_REGB BITS */
#define ST				0x20
#define ENABLE			0x40
#define CLKHLD			0x80

/* Device Meta Data */
#define DESC_DEV		"KXUD9 3-axis Accelerometer"	// Device Description
#define VERSION_DEV		"1.1.8"
#define VER_MAJOR_DEV	1
#define	VER_MINOR_DEV	1
#define VER_MAINT_DEV	8
#define	MAX_G_DEV		(8.0f)		// Maximum G Level
#define	MAX_SENS_DEV	(819.0f)	// Maximum Sensitivity
#define PWR_DEV			(0.03f)		// Typical Current

/* Input Device Name */
#define INPUT_NAME_ACC	"gsensor"

/* Device name for kxud9 misc. device */
#define NAME_DEV	"kxud9"
#define DIR_DEV		"/dev/kxud9"

/* IOCTLs for kxud9 misc. device library */
#define KXUD9IO									0x96
#define KXUD9_IOCTL_GET_COUNT			_IOR(KXUD9IO, 0x01, int)
#define KXUD9_IOCTL_GET_MG				_IOR(KXUD9IO, 0x02, int)
#define KXUD9_IOCTL_ENABLE_OUTPUT		 _IO(KXUD9IO, 0x03)
#define KXUD9_IOCTL_DISABLE_OUTPUT		 _IO(KXUD9IO, 0x04)
#define KXUD9_IOCTL_GET_ENABLE			_IOR(KXUD9IO, 0x05, int)
#define KXUD9_IOCTL_RESET				 _IO(KXUD9IO, 0x06)
#define KXUD9_IOCTL_UPDATE_ODR			_IOW(KXUD9IO, 0x07, int)


#ifdef __KERNEL__
struct kxud9_platform_data {
	int poll_interval;
	int min_interval;
	/* the desired g-range, in milli-g */
	u8 g_range;
	/* used to compensate for alternate device placement within the host */
	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;
	u8 negate_x;
	u8 negate_y;
	u8 negate_z;

	/* initial configuration values, set during board configuration */
	u8 ctrl_regc_init;
	u8 ctrl_regb_init;

	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);
};
#endif /* __KERNEL__ */

#endif  /* __KXUD9_H__ */

