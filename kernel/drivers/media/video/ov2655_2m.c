/* linux/drivers/media/video/ov2655.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *	         http://www.samsung.com/
 *
 * Driver for S5K4BA (UXGA camera) from Samsung Electronics
 * 1/4" 2.0Mp CMOS Image Sensor SoC with an Embedded Image Processor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
//#include <media/v4l2-i2c-drv.h>
#include <media/ov2655_platform.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include "ov2655_2m.h"

#define ov2655_DRIVER_NAME	"ov2655"

/* Default resolution & pixelformat. plz ref ov2655_platform.h */
#define DEFAULT_RES		WVGA	/* Index of resoultion */
#define DEFAUT_FPS_INDEX	OV2655_15FPS
#define DEFAULT_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */

#define HERE    printk("x4412: %s, %d\n", __FUNCTION__, __LINE__);

/*
 * Specification
 * Parallel : ITU-R. 656/601 YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Serial : MIPI CSI2 (single lane) YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Resolution : 1280 (H) x 1024 (V)
 * Image control : Brightness, Contrast, Saturation, Sharpness, Glamour
 * Effect : Mono, Negative, Sepia, Aqua, Sketch
 * FPS : 15fps @full resolution, 30fps @VGA, 24fps @720p
 * Max. pixel clock frequency : 48MHz(upto)
 * Internal PLL (6MHz to 27MHz input frequency)
 */

static int ov2655_init(struct v4l2_subdev *sd, u32 val);
/* Camera functional setting values configured by user concept */
struct ov2655_userset
{
	signed int exposure_bias; /* V4L2_CID_EXPOSURE */
	unsigned int ae_lock;
	unsigned int awb_lock;
	unsigned int auto_wb; /* V4L2_CID_CAMERA_WHITE_BALANCE */
	unsigned int manual_wb; /* V4L2_CID_WHITE_BALANCE_PRESET */
	unsigned int wb_temp; /* V4L2_CID_WHITE_BALANCE_TEMPERATURE */
	unsigned int effect; /* Color FX (AKA Color tone) */
	unsigned int contrast; /* V4L2_CID_CAMERA_CONTRAST */
	unsigned int saturation; /* V4L2_CID_CAMERA_SATURATION */
	unsigned int sharpness; /* V4L2_CID_CAMERA_SHARPNESS */
	unsigned int glamour;
};

struct ov2655_state
{
	struct ov2655_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	struct ov2655_userset userset;
	enum v4l2_pix_format_mode format_mode;
	int framesize_index;
	int freq; /* MCLK in KHz */
	int is_mipi;
	int isize;
	int ver;
	int fps;
	int check_previewdata;
};

enum
{
	S5K4BA_PREVIEW_SVGA,
};

struct ov2655_enum_framesize
{
	unsigned int index;
	unsigned int width;
	unsigned int height;
};

struct ov2655_enum_framesize ov2655_framesize_list[] = {
	{ S5K4BA_PREVIEW_SVGA, 800, 600 }
};

static inline struct ov2655_state *to_state(struct v4l2_subdev *sd)
{
return container_of(sd, struct ov2655_state, sd);
}

/*
 * S5K4BA register structure : 2bytes address, 2bytes value
 * retry on write failure up-to 5 times
 */
static inline int OV2655_write(struct v4l2_subdev *sd, unsigned short addr,
		u8 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg[1];
	unsigned char reg[3];

	if (!client->adapter)
		return -ENODEV;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 3;
	msg->buf = reg;

	reg[0] = addr >> 8;
	reg[1] = addr & 0xFF;
	reg[2] = val & 0xFF;

	if (i2c_transfer(client->adapter, msg, 1) != 1)
	{
		printk("ov2655 i2c_transfer failed\n");
		return -EIO;
	}

	return 0;
}

static int __OV2655_init_2bytes(struct v4l2_subdev *sd, unsigned short *reg[],
		int total)
{
	//struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = -EINVAL, i;
	unsigned short *item;
	//unsigned char bytes;

	for (i = 0; i < total; i++)
	{
		item = (unsigned short *) &reg[i];

		if (item[0] == REG_DELAY)
		{
			mdelay(item[1]);
			ret = 0;
		}
		/*else if (item[0] == REG_ID)
		 {
		 DDI_I2C_Read(sd, 0x0001, 2, &bytes, 1); // 1byte read
		 printk("===== cam sensor ov2655 ID : 0x%x ", bytes);
		 DDI_I2C_Read(sd, 0x0002, 2, &bytes, 1); // 1byte read
		 }*/
		else
		{
			ret = OV2655_write(sd, item[0], item[1]);
		}

		if (ret < 0)
			;//v4l_info(client, "%s: register set failed\n", __func__);
	}

	return ret;
}

static const char *ov2655_querymenu_wb_preset[] =
{ "WB Tungsten", "WB Fluorescent", "WB sunny", "WB cloudy", NULL };

static const char *ov2655_querymenu_effect_mode[] =
{ "Effect Sepia", "Effect Aqua", "Effect Monochrome", "Effect Negative",
		"Effect Sketch", NULL };

static const char *ov2655_querymenu_ev_bias_mode[] =
{ "-3EV", "-2,1/2EV", "-2EV", "-1,1/2EV", "-1EV", "-1/2EV", "0", "1/2EV",
		"1EV", "1,1/2EV", "2EV", "2,1/2EV", "3EV", NULL };

static struct v4l2_queryctrl ov2655_controls[] =
{
	{
		/*
		 * For now, we just support in preset type
		 * to be close to generic WB system,
		 * we define color temp range for each preset
		 */
		.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "White balance in kelvin",
		.minimum = 0,
		.maximum = 10000,
		.step = 1,
		.default_value = 0, /* FIXME */
	},
	{
		.id = V4L2_CID_WHITE_BALANCE_PRESET,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "White balance preset",
		.minimum = 0,
		.maximum = ARRAY_SIZE(ov2655_querymenu_wb_preset) - 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_CAMERA_WHITE_BALANCE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Auto white balance",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_EXPOSURE,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Exposure bias",
		.minimum = 0,
		.maximum = ARRAY_SIZE(ov2655_querymenu_ev_bias_mode) - 2,
		.step = 1,
		.default_value =
		(ARRAY_SIZE(ov2655_querymenu_ev_bias_mode) - 2) / 2,
		/* 0 EV */
	},
	{
		.id = V4L2_CID_CAMERA_EFFECT,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Image Effect",
		.minimum = 0,
		.maximum = ARRAY_SIZE(ov2655_querymenu_effect_mode) - 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_CAMERA_CONTRAST,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Contrast",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
	{
		.id = V4L2_CID_CAMERA_SATURATION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Saturation",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
	{
		.id = V4L2_CID_CAMERA_SHARPNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Sharpness",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
};

const char **ov2655_ctrl_get_menu(u32 id)
{
	HERE
	switch (id)
	{
	case V4L2_CID_WHITE_BALANCE_PRESET:
		return ov2655_querymenu_wb_preset;

	case V4L2_CID_CAMERA_EFFECT:
		return ov2655_querymenu_effect_mode;

	case V4L2_CID_EXPOSURE:
		return ov2655_querymenu_ev_bias_mode;

	default:
		return v4l2_ctrl_get_menu(id);
	}
}

static inline struct v4l2_queryctrl const *ov2655_find_qctrl(int id)
{
	int i;
	HERE
	for (i = 0; i < ARRAY_SIZE(ov2655_controls); i++)
		if (ov2655_controls[i].id == id)
			return &ov2655_controls[i];

	return NULL;
}

static int ov2655_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	//return 0;
	int i;
	HERE
	for (i = 0; i < ARRAY_SIZE(ov2655_controls); i++)
	{
		if (ov2655_controls[i].id == qc->id)
		{
			memcpy(qc, &ov2655_controls[i], sizeof(struct v4l2_queryctrl));
			return 0;
		}
	}

	return -EINVAL;
}

static int ov2655_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	//return 0;
	struct v4l2_queryctrl qctrl;
	HERE
	qctrl.id = qm->id;
	ov2655_queryctrl(sd, &qctrl);

	return v4l2_ctrl_query_menu(qm, &qctrl, ov2655_ctrl_get_menu(qm->id));
}

static int ov2655_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *ffmt)
{
	struct ov2655_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	HERE
	v4l_info(client, "%s:ffmt->colorspace=%d\n", __func__,ffmt->colorspace);
	printk("%s:ffmt->colorspace=%d\n", __func__,ffmt->colorspace);
	
	if (ffmt->colorspace == V4L2_COLORSPACE_JPEG) {
		state->format_mode = V4L2_PIX_FMT_MODE_CAPTURE;	
	//	u_index = 1;
	} else {
		state->format_mode = V4L2_PIX_FMT_MODE_PREVIEW;
		//u_index = 0;
	}
	return 0;
}

static int ov2655_s_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;//lqm.
	struct ov2655_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	HERE
	v4l_info(client, "%s:enable=%d,state->format_mode=%d\n", __func__,enable,state->format_mode);

	switch (enable) {
	case STREAM_MODE_CAM_ON:
	case STREAM_MODE_CAM_OFF:
		switch (state->format_mode) {
		case V4L2_PIX_FMT_MODE_CAPTURE:			
			break;
		case V4L2_PIX_FMT_MODE_PREVIEW:			
			break;
		case V4L2_PIX_FMT_MODE_HDR:			
			break;
		default:
			break;
		}
		break;

	case STREAM_MODE_MOVIE_ON:
		break;

	case STREAM_MODE_MOVIE_OFF:
		break;

	default:
		printk("invalid stream option, %d\n", enable);
		break;
	}

	return 0;
}

static int ov2655_enum_framesizes(struct v4l2_subdev *sd,
		struct v4l2_frmsizeenum *fsize)
{
	struct ov2655_state *state = to_state(sd);
	int num_entries = sizeof(ov2655_framesize_list)
			/ sizeof(struct ov2655_enum_framesize);
	struct ov2655_enum_framesize *elem;
	int index = 0;
	int i = 0;
	HERE
	/* The camera interface should read this value, this is the resolution
	 * at which the sensor would provide framedata to the camera i/f
	 *
	 * In case of image capture,
	 * this returns the default camera resolution (WVGA)
	 */
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

	index = state->framesize_index;

	fsize->discrete.width = ov2655_framesize_list[0].width;
	fsize->discrete.height = ov2655_framesize_list[0].height;
	printk("x4412:fsize->discrete.width=%d,fsize->discrete.height=%d\n",
					fsize->discrete.width,fsize->discrete.height);
	return 0;

	for (i = 0; i < num_entries; i++)
	{
		elem = &ov2655_framesize_list[i];
		if (elem->index == index)
		{
			fsize->discrete.width = ov2655_framesize_list[index].width;
			fsize->discrete.height = ov2655_framesize_list[index].height;
			printk("x4412:fsize->discrete.width=%d,fsize->discrete.height=%d\n",
					fsize->discrete.width,fsize->discrete.height);
			return 0;
		}
	}

	return -EINVAL;
}

static int ov2655_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;
	HERE
	dev_dbg(&client->dev, "%s\n", __func__);

	return err;
}

static int ov2655_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;
	HERE
	dev_dbg(&client->dev, "%s: numerator %d, denominator: %d\n", __func__,
			param->parm.capture.timeperframe.numerator,
			param->parm.capture.timeperframe.denominator);

	return err;
}

static int ov2655_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	//return 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov2655_state *state = to_state(sd);
	struct ov2655_userset userset = state->userset;
	int err = 0;
	HERE
	switch (ctrl->id)
	{
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		ctrl->value = userset.auto_wb;
		break;
	case V4L2_CID_WHITE_BALANCE_PRESET:
		ctrl->value = userset.manual_wb;
		break;
	case V4L2_CID_CAMERA_EFFECT:
		ctrl->value = userset.effect;
		break;
	case V4L2_CID_CAMERA_CONTRAST:
		ctrl->value = userset.contrast;
		break;
	case V4L2_CID_CAMERA_SATURATION:
		ctrl->value = userset.saturation;
		break;
	case V4L2_CID_CAMERA_SHARPNESS:
		ctrl->value = userset.saturation;
		break;
	case V4L2_CID_CAM_JPEG_MAIN_SIZE:
		ctrl->value = 0;
		break;
	case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
		ctrl->value = 0;
		break;
	case V4L2_CID_CAM_JPEG_THUMB_SIZE:
		ctrl->value = 0;
		break;
	case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
		ctrl->value = 0;
		break;
	case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
		ctrl->value = 0;
		break;
	case V4L2_CID_CAM_JPEG_MEMSIZE:
		ctrl->value = 0x410580;//SENSOR_JPEG_SNAPSHOT_MEMSIZE;
		break;
	case V4L2_CID_CAM_JPEG_QUALITY:
		ctrl->value = 100;
		break;
	//case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT_FIRST:
	case V4L2_CID_CAM_DATE_INFO_YEAR:
	case V4L2_CID_CAM_DATE_INFO_MONTH:
	case V4L2_CID_CAM_DATE_INFO_DATE:
	case V4L2_CID_CAM_SENSOR_VER:
	case V4L2_CID_CAM_FW_MINOR_VER:
	case V4L2_CID_CAM_FW_MAJOR_VER:
	case V4L2_CID_CAM_PRM_MINOR_VER:
	case V4L2_CID_CAM_PRM_MAJOR_VER:
	//case V4L2_CID_ESD_INT:
	//case V4L2_CID_CAMERA_GET_ISO:
	//case V4L2_CID_CAMERA_GET_SHT_TIME:
	case V4L2_CID_CAMERA_OBJ_TRACKING_STATUS:
	case V4L2_CID_CAMERA_SMART_AUTO_STATUS:
		ctrl->value = 0;
		//ctrl->value = 0x3A0000 +0xFC00 + 0xBB800;//lqm.
		break;
	case V4L2_CID_EXPOSURE:
		ctrl->value = userset.exposure_bias;
		break;
	default:
		dev_err(&client->dev, "%s: no such ctrl\n", __func__);
		/* err = -EINVAL; */
		break;
	}

	return err;
}

static int ov2655_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov2655_state *state = to_state(sd);
	int err = -EINVAL;
	HERE
	v4l_info(client, "%s: camera initialization start\n", __func__);

	state->format_mode = V4L2_PIX_FMT_MODE_PREVIEW;

	err = __OV2655_init_2bytes(sd, (unsigned short **) ov2655_init_reg,
			OV2655_INIT_REGS);

	if (err < 0)
	{
		/* This is preview fail */
		state->check_previewdata = 100;
		v4l_err(client, "%s: camera initialization failed. err(%d)\n",
				__func__, state->check_previewdata);
		return -EIO;
	}

	/* This is preview success */
	state->check_previewdata = 0;
	return 0;
}

static const struct v4l2_subdev_core_ops ov2655_core_ops =
{
	.init = ov2655_init, /* initializing API */
	.queryctrl = ov2655_queryctrl,
	.querymenu = ov2655_querymenu,
	.g_ctrl = ov2655_g_ctrl,
};

static const struct v4l2_subdev_video_ops ov2655_video_ops =
{
	.enum_framesizes = ov2655_enum_framesizes,
	.g_parm = ov2655_g_parm,
	.s_parm = ov2655_s_parm,
	.s_mbus_fmt = ov2655_s_fmt,
	.s_stream = ov2655_s_stream,
};

static const struct v4l2_subdev_ops ov2655_ops =
{
	.core = &ov2655_core_ops,
	.video = &ov2655_video_ops,
};

/*
 * ov2655_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int ov2655_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct ov2655_state *state;
	struct v4l2_subdev *sd;

	state = kzalloc(sizeof(struct ov2655_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	sd = &state->sd;
	strcpy(sd->name, ov2655_DRIVER_NAME);

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &ov2655_ops);

	dev_info(&client->dev, "ov2655 has been probed\n");
	return 0;
}

static int ov2655_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id ov2655_id[] = {
	{ ov2655_DRIVER_NAME, 0 },
	{ }, 
};
MODULE_DEVICE_TABLE(i2c, ov2655_id);

static struct i2c_driver ov2655_i2c_driver =
{
	.driver = {
		.name	= ov2655_DRIVER_NAME,
	},
	.probe = ov2655_probe,
	.remove = ov2655_remove,
	.id_table = ov2655_id,
};

static int __init ov2655_mod_init(void)
{
	return i2c_add_driver(&ov2655_i2c_driver);
}

static void __exit ov2655_mod_exit(void)
{
	i2c_del_driver(&ov2655_i2c_driver);
}

module_init(ov2655_mod_init);
module_exit(ov2655_mod_exit);

MODULE_DESCRIPTION("Samsung Electronics S5K4BA UXGA camera driver");
MODULE_AUTHOR("Jinsung Yang <jsgood.yang@samsung.com>");
MODULE_LICENSE("GPL");
