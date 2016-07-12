/*
 * driver for 9tripod ov2655 2MP camera
 *
 * Copyright (c) 2010, Samsung Electronics. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <linux/i2c.h>
#include <linux/init.h>
#include <media/v4l2-device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/videodev2.h>
#include <linux/io.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include <linux/regulator/machine.h>

#include <media/ov2655_platform.h>
#include "ov2655.h"


#include <mach/gpio.h>
#include <plat/gpio-cfg.h>


#define CONTINUOUS_FOCUS
#define OV2655_DRIVER_NAME	"ov2655"

#define OV2655_JPEG_MAXSIZE	0x3A0000
#define OV2655_THUMB_MAXSIZE	0xFC00
#define OV2655_POST_MAXSIZE	0xBB800

#define CHECK_ERR(x)	if ((x) < 0) { \
				cam_err("i2c failed, err %d\n", x); \
				return x; \
			}

static struct i2c_client *gclient;

//static int ghm5065 = 0;
//static int cam_count = 0;
static int gCameraId = 0;

int WhiteBanlanceValue;
static int gIs_30W_capture = 1;
static int gIs_720P_capture = 0;
static int gIs_200W_capture = 0;
static int gIs_500W_capture = 0;
static int g_cam_model=0;
static int cam_sw_720p=0;
static int cam_sw_vga=0;
static int cam_sw_flag=0;

static const struct ov2655_frmsizeenum preview_frmsizes[] = {
	{ OV2655_PREVIEW_QCIF,	176,	144,	0x05 },	/* 176 x 144 */
	{ OV2655_PREVIEW_QCIF2,	528,	432,	0x2C },	/* 176 x 144 */
	{ OV2655_PREVIEW_QVGA,	320,	240,	0x09 },
	{ OV2655_PREVIEW_VGA,	640,	480,	0x17 },
	{ OV2655_PREVIEW_D1,	720,	480,	0x18 },
	{ OV2655_PREVIEW_WVGA,	800,	480,	0x1A },
	{ OV2655_PREVIEW_720P,	1280,	720,	0x21 },
	{ OV2655_PREVIEW_1080P,	1920,	1080,	0x28 },
	{ OV2655_PREVIEW_HDR,	3264,	2448,	0x27 },
};

static const struct ov2655_frmsizeenum capture_frmsizes[] = {
	{ OV2655_CAPTURE_VGA,	640,	480,	0x09 },
	{ OV2655_CAPTURE_WVGA,	800,	480,	0x0A },
	{ OV2655_CAPTURE_W1MP,	1600,	960,	0x2C },
	{ OV2655_CAPTURE_2MP,	1600,	1200,	0x2C },//jerry
	{ OV2655_CAPTURE_W2MP,	2048,	1232,	0x2C },
	{ OV2655_CAPTURE_3MP,	2048,	1536,	0x1B },
	{ OV2655_CAPTURE_W4MP,	2560,	1536,	0x1B },
	{ OV2655_CAPTURE_5MP,	2592,	1944,	0x1B },
	{ OV2655_CAPTURE_W6MP,	3072,	1856,	0x1B },	
	{ OV2655_CAPTURE_7MP,	3072,	2304,	0x2D },
	{ OV2655_CAPTURE_W7MP,	2560,	1536,	0x2D },
	{ OV2655_CAPTURE_8MP,	3264,	2448,	0x25 },
};

static inline struct ov2655_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov2655_state, sd);
}

static int ov2655_set_mode(struct v4l2_subdev *sd, u32 mode)
{
	int err;
	cam_trace("E\n");
	
	switch (1) {
	case OV2655_SYSINIT_MODE:
		cam_warn("sensor is initializing\n");
		err = -EBUSY;
		break;

	case OV2655_PARMSET_MODE:
	case OV2655_MONITOR_MODE:
	case OV2655_STILLCAP_MODE:
		break;

	default:
		err = -EINVAL;
	}



	cam_trace("X\n");
	return 0;
}

static int ov2655_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct ov2655_state *state = to_state(sd);
	int err = 0;
	switch (ctrl->id) {
	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
		break;

	case V4L2_CID_CAM_JPEG_MEMSIZE:
		ctrl->value = OV2655_JPEG_MAXSIZE +
			OV2655_THUMB_MAXSIZE + OV2655_POST_MAXSIZE;
		break;

	case V4L2_CID_CAM_JPEG_MAIN_SIZE:
		ctrl->value = state->jpeg.main_size;
		break;

	case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
		ctrl->value = state->jpeg.main_offset;
		break;

	case V4L2_CID_CAM_JPEG_THUMB_SIZE:
		ctrl->value = state->jpeg.thumb_size;
		break;

	case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
		ctrl->value = state->jpeg.thumb_offset;
		break;

	case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
		ctrl->value = state->jpeg.postview_offset;
		break;

	case V4L2_CID_CAMERA_EXIF_FLASH:
		ctrl->value = state->exif.flash;
		break;

	case V4L2_CID_CAMERA_EXIF_ISO:
		ctrl->value = state->exif.iso;
		break;

	case V4L2_CID_CAMERA_EXIF_TV:
		ctrl->value = state->exif.tv;
		break;

	case V4L2_CID_CAMERA_EXIF_BV:
		ctrl->value = state->exif.bv;
		break;

	case V4L2_CID_CAMERA_EXIF_EBV:
		ctrl->value = state->exif.ebv;
		break;	

	case V4L2_CID_CAMERA_MODEL:
		ctrl->value = g_cam_model;
		break;

	default:
		cam_err("no such control id %d\n",
				ctrl->id - V4L2_CID_PRIVATE_BASE);
		/*err = -ENOIOCTLCMD*/
		err = 0;
		break;
	}

	if (err < 0 && err != -ENOIOCTLCMD)
		cam_err("failed, id %d\n", ctrl->id - V4L2_CID_PRIVATE_BASE);

	return err;
}

static int ov2655_set_flash(struct v4l2_subdev *sd, int val, int recording)
{
	struct ov2655_state *state = to_state(sd);
	int light, flash;
	cam_dbg("E, value %d\n", val);

	if (!recording)
		state->flash_mode = val;
	return 0;

	/* movie flash mode should be set when recording is started */
	if (state->sensor_mode == SENSOR_MOVIE && !recording)
		return 0;

retry:
	switch (val) {
	case FLASH_MODE_OFF:
		light = 0x00;
		flash = (state->sensor_mode == SENSOR_CAMERA) ? 0x00 : -1;
		break;

	case FLASH_MODE_AUTO:
		light = (state->sensor_mode == SENSOR_CAMERA) ? 0x02 : 0x04;
		flash = (state->sensor_mode == SENSOR_CAMERA) ? 0x02 : -1;
		break;

	case FLASH_MODE_ON:
		light = (state->sensor_mode == SENSOR_CAMERA) ? 0x01 : 0x03;
		flash = (state->sensor_mode == SENSOR_CAMERA) ? 0x01 : -1;
		break;

	case FLASH_MODE_TORCH:
		light = 0x03;
		flash = -1;
		break;

	default:
		cam_warn("invalid value, %d\n", val);
		val = FLASH_MODE_OFF;
		goto retry;
	}

	if (light >= 0) {
	}

	if (flash >= 0) {
	}

	cam_trace("X\n");
	return 0;
}

static int ov2655_set_af(struct v4l2_subdev *sd, int val)
{
	struct ov2655_state *state = to_state(sd);
	int err=0;

	state->focus.status = 0;

	cam_dbg("X\n");
	return err;
}

static int ov2655_set_zoom(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct ov2655_state *state = to_state(sd);
	struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value;
	cam_dbg("E, value %d\n", val);

	qc.id = ctrl->id;

	if (val < qc.minimum || val > qc.maximum) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}


	state->zoom = val;

	cam_trace("X\n");
	return 0;
}

static int ov2655_g_ext_ctrl(struct v4l2_subdev *sd, struct v4l2_ext_control *ctrl)
{
	struct ov2655_state *state = to_state(sd);
	int err = 0;
	printk("********[ov2655_g_ext_ctrl][ctrl->id=%d] [ctrl->value=%d]\n",ctrl->id,ctrl->value);

	switch (ctrl->id) {
	case V4L2_CID_CAM_SENSOR_FW_VER:
		strcpy(ctrl->string, state->exif.unique_id);
		break;

	default:
		cam_err("no such control id %d\n", ctrl->id - V4L2_CID_CAMERA_CLASS_BASE);
		/*err = -ENOIOCTLCMD*/
		err = 0;
		break;
	}

	if (err < 0 && err != -ENOIOCTLCMD)
		cam_err("failed, id %d\n", ctrl->id - V4L2_CID_CAMERA_CLASS_BASE);

	return err;
}

static int ov2655_g_ext_ctrls(struct v4l2_subdev *sd, struct v4l2_ext_controls *ctrls)
{
	struct v4l2_ext_control *ctrl = ctrls->controls;
	int i, err = 0;

	for (i = 0; i < ctrls->count; i++, ctrl++) {
		err = ov2655_g_ext_ctrl(sd, ctrl);
		printk("########[ov2655_g_ext_ctrls][count=%d][ctrl->value=%d]\n",ctrls->count,ctrl->value);
		if (err) {
			ctrls->error_idx = i;
			break;
		}
	}
	return err;
}
/*
 * v4l2_subdev_video_ops
 */
static const struct ov2655_frmsizeenum *ov2655_get_frmsize
	(const struct ov2655_frmsizeenum *frmsizes, int num_entries, int index)
{
	int i;

	for (i = 0; i < num_entries; i++) {
		if (frmsizes[i].index == index)
			return &frmsizes[i];
	}

	return NULL;
}

static int ov2655_set_frmsize(struct v4l2_subdev *sd)
{
	struct ov2655_state *state = to_state(sd);
	struct v4l2_control ctrl;
	int err;

	if (state->format_mode != V4L2_PIX_FMT_MODE_CAPTURE) {
		err = ov2655_set_mode(sd, OV2655_PARMSET_MODE);

		CHECK_ERR(err);


		if (state->zoom) {
			/* Zoom position returns to 1 when the monitor size is changed. */
			ctrl.id = V4L2_CID_CAMERA_ZOOM;
			ctrl.value = state->zoom;
			ov2655_set_zoom(sd, &ctrl);
		}
	}

	cam_trace("X\n");
	return 0;
}
static int ov2655_enum_framesizes(struct v4l2_subdev *sd,
	struct v4l2_frmsizeenum *fsize);

static int ov2655_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *ffmt)
{
	struct ov2655_state *state = to_state(sd);
	const struct ov2655_frmsizeenum **frmsize;
	struct v4l2_frmsizeenum cam_frmsize;

#if 1
	u32 width = ffmt->width;
	u32 height = ffmt->height;
	u32 tmp_width;
	u32 old_index;
	int i, num_entries;
	cam_frmsize.discrete.width = 0;
	cam_frmsize.discrete.height = 0;
	cam_trace("E\n");
	
	if(width==1280 && cam_sw_flag==0)	
	{
		cam_sw_flag=1;
		cam_sw_720p=1;
	}
	if(width==640 && cam_sw_flag==1)	
	{
		cam_sw_vga=1;
		cam_sw_flag=0;
	}
	if (ffmt->field==2)
	{
		if (ffmt->width == 640 && ffmt->height == 480)
		{
			gIs_30W_capture = 1;
			gIs_720P_capture = 0;
			gIs_200W_capture = 0;
			gIs_500W_capture = 0;
			ov2655_enum_framesizes(sd,&cam_frmsize);
			mdelay(50);
		}
		else if (ffmt->width == 1600 && ffmt->height == 1200)
		{
			gIs_30W_capture =  0;
			gIs_720P_capture = 0;
			gIs_200W_capture = 1;
			gIs_500W_capture = 0;
			ov2655_enum_framesizes(sd,&cam_frmsize);
			mdelay(50);
		}
		else if (ffmt->width == 2592 && ffmt->height == 1944)
		{
			gIs_30W_capture =  0;
			gIs_720P_capture = 0;
			gIs_200W_capture = 0;
			gIs_500W_capture = 1;			
			ov2655_enum_framesizes(sd,&cam_frmsize);
			mdelay(50);
		}
	}
#endif

	#if 1
	if (unlikely(state->isp.bad_fw)) {
		cam_err("\"Unknown\" state, please update F/W");
		return -ENOSYS;
	}
	if (ffmt->width < ffmt->height) {
		tmp_width = ffmt->height;
		height = ffmt->width;
		width = tmp_width;
	}

	if (ffmt->colorspace == V4L2_COLORSPACE_JPEG) {
		state->format_mode = V4L2_PIX_FMT_MODE_CAPTURE;
		frmsize = &state->capture;
	} else {
		state->format_mode = V4L2_PIX_FMT_MODE_PREVIEW;
		frmsize = &state->preview;
	}
	
	old_index = *frmsize ? (*frmsize)->index : -1;
	*frmsize = NULL;

	if (state->format_mode != V4L2_PIX_FMT_MODE_CAPTURE) {		
		num_entries = ARRAY_SIZE(preview_frmsizes);
		for (i = 0; i < num_entries; i++) {
			if (width == preview_frmsizes[i].width &&
				height == preview_frmsizes[i].height) {
				*frmsize = &preview_frmsizes[i];
				break;
			}
		}
		printk("======== preview_frmsizes =========>i=%d width=%d height=%d\n",i,preview_frmsizes[i].width,preview_frmsizes[i].height);
	} else {
		num_entries = ARRAY_SIZE(capture_frmsizes);
		for (i = 0; i < num_entries; i++) {
			if (width == capture_frmsizes[i].width &&
				height == capture_frmsizes[i].height) {
				*frmsize = &capture_frmsizes[i];
				break;
			}
		}
		printk("======== capture_frmsizes =========>i=%d width=%d height=%d\n",i,capture_frmsizes[i].width,capture_frmsizes[i].height);
	}

	if (*frmsize == NULL) {
		cam_warn("invalid frame size %dx%d\n", width, height);
		*frmsize = state->format_mode != V4L2_PIX_FMT_MODE_CAPTURE ?
			ov2655_get_frmsize(preview_frmsizes, num_entries,
				OV2655_PREVIEW_VGA) :
			ov2655_get_frmsize(capture_frmsizes, num_entries,
				OV2655_CAPTURE_8MP);
	}

	cam_dbg("%dx%d\n", (*frmsize)->width, (*frmsize)->height);
	ov2655_set_frmsize(sd);

	cam_trace("X\n");
	#endif
	return 0;
}

static int ov2655_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct ov2655_state *state = to_state(sd);

	a->parm.capture.timeperframe.numerator = 1;
	a->parm.capture.timeperframe.denominator = state->fps;

	return 0;
}

static int ov2655_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct ov2655_state *state = to_state(sd);
	int err;

	u32 fps = a->parm.capture.timeperframe.denominator /
					a->parm.capture.timeperframe.numerator;

	if (unlikely(state->isp.bad_fw)) {
		cam_err("\"Unknown\" state, please update F/W");
		return -ENOSYS;
	}

	if (fps != state->fps) {
		if (fps <= 0 || fps > 30) {
			cam_err("invalid frame rate %d\n", fps);
			fps = 30;
		}
		state->fps = fps;
	}

	err = ov2655_set_mode(sd, OV2655_PARMSET_MODE);
	CHECK_ERR(err);

	cam_dbg("fixed fps %d\n", state->fps);

	return 0;
}

static int ov2655_enum_framesizes(struct v4l2_subdev *sd,
	struct v4l2_frmsizeenum *fsize)
{
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	if (gclient->addr == 0x24 || gclient->addr == 0x20)
		{
			if (gIs_30W_capture)
			{
					fsize->discrete.width = 800;	
					fsize->discrete.height = 600;
			}
			else if(gIs_720P_capture)
			{
					fsize->discrete.width = 1280;	
					fsize->discrete.height = 720;
			}
			else if(gIs_200W_capture)
			{
					fsize->discrete.width = 1600;	
					fsize->discrete.height = 1200;
			}			
		}
	else if (gclient->addr == 0x3c)
		{
			if (!gIs_200W_capture)
			{
					fsize->discrete.width = 640;	
					fsize->discrete.height = 480;
			}
			else
			{
					fsize->discrete.width = 2592;	
					fsize->discrete.height = 1944;
			}
		}
	else if (gclient->addr == 0x1f)
		{
			if (gIs_30W_capture)
			{
					fsize->discrete.width = 640;	
					fsize->discrete.height = 480;
			}
			else if(gIs_720P_capture)
			{
					fsize->discrete.width = 1280;	
					fsize->discrete.height = 720;
			}			
			else if(gIs_200W_capture)
			{
					fsize->discrete.width = 1600;	
					fsize->discrete.height = 1200;
			}
			else if(gIs_500W_capture)
			{
					fsize->discrete.width = 2592;
					fsize->discrete.height = 1944;
//					fsize->discrete.height = 1900;
			}
		}
	else
		{
			fsize->discrete.width = 640;	
			fsize->discrete.height = 480;
		}
	
	printk("fsize->discrete.width=%d fsize->discrete.height=%d\n" ,fsize->discrete.width,fsize->discrete.height);

	return 0;
}

static int ov2655_s_stream_preview(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static int ov2655_s_stream_capture(struct v4l2_subdev *sd, int enable)
{
	int cap_index=0;
	struct ov2655_state *state = to_state(sd);
	struct v4l2_frmsizeenum cam_frmsize;
	printk("========>%s index=%d enable=%d\n",__func__,state->capture->index,enable);
	cap_index=state->capture->index;
	cam_frmsize.discrete.width = 0;
	cam_frmsize.discrete.height = 0;

	if (enable) {
#if 1
		if(state->capture->index == 0)
		{
			gIs_30W_capture = 1;
			gIs_200W_capture = 0;
			gIs_500W_capture = 0;
		}
		else if(state->capture->index == 3){
			gIs_30W_capture = 0;
			gIs_200W_capture = 1;
			gIs_500W_capture = 0;
		}
		else if(state->capture->index == 7){
			gIs_30W_capture = 0;
			gIs_200W_capture = 0;
			gIs_500W_capture = 1;
		}
#endif		
	} 
	else {
#if 1
		gIs_30W_capture = 1;
		gIs_200W_capture = 0;
		gIs_500W_capture = 0;
#endif
	}

	return 0;
}

static int ov2655_s_stream_hdr(struct v4l2_subdev *sd, int enable)
{
	int err;

	err = ov2655_set_mode(sd, OV2655_PARMSET_MODE);
	CHECK_ERR(err);

	if (enable) {
	} else {
	}
	return 0;
}

static int ov2655_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov2655_state *state = to_state(sd);
	int err;
	cam_trace("E\n");

	if (unlikely(state->isp.bad_fw)) {
		cam_err("\"Unknown\" state, please update F/W");
		return -ENOSYS;
	}

	switch (enable) {
	case STREAM_MODE_CAM_ON:
	case STREAM_MODE_CAM_OFF:
		switch (state->format_mode) {
		case V4L2_PIX_FMT_MODE_CAPTURE:
			printk("capture %s",
				enable == STREAM_MODE_CAM_ON ? "on" : "off");
			err = ov2655_s_stream_capture(sd, enable == STREAM_MODE_CAM_ON);
			break;
		case V4L2_PIX_FMT_MODE_HDR:
			err = ov2655_s_stream_hdr(sd, enable == STREAM_MODE_CAM_ON);
			break;
		default:
			printk("preview %s",
				enable == STREAM_MODE_CAM_ON ? "on" : "off");
			err = ov2655_s_stream_preview(sd, enable == STREAM_MODE_CAM_ON);
			break;
		}
		break;
 
	case STREAM_MODE_MOVIE_ON:
/*		if (state->flash_mode != FLASH_MODE_OFF)
			err = ov2655_set_flash(sd, state->flash_mode, 1);

		if (state->preview->index == OV2655_PREVIEW_720P ||
				state->preview->index == OV2655_PREVIEW_1080P)
			err = ov2655_set_af(sd, 1);
*/
		break;

	case STREAM_MODE_MOVIE_OFF:
/*		if (state->preview->index == OV2655_PREVIEW_720P ||
				state->preview->index == OV2655_PREVIEW_1080P)
			err = ov2655_set_af(sd, 0);

		ov2655_set_flash(sd, FLASH_MODE_OFF, 1);
*/
		break;

	default:
		cam_err("invalid stream option, %d\n", enable);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static inline int OV2655_write(struct v4l2_subdev *sd, unsigned short addr,u8 val)
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

static int OV2655_init_2bytes(struct v4l2_subdev *sd, unsigned short *reg[],
		int total)
{
	int ret = -EINVAL, i;
	unsigned short *item;

	for (i = 0; i < total; i++)
	{
		item = (unsigned short *) &reg[i];

		if (item[0] == REG_DELAY)
		{
			mdelay(item[1]);
			ret = 0;
		}
		else
		{
			ret = OV2655_write(sd, item[0], item[1]);
		}

		if (ret < 0)
			;
	}

	return ret;
}

static int ov2655_init(struct v4l2_subdev *sd, u32 val)
{

	struct ov2655_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	//takepicture exceptionally foce quit
	struct v4l2_frmsizeenum cam_frmsize;
	cam_frmsize.discrete.width = 0;
	cam_frmsize.discrete.height = 0;	
	err = 0;
	
	gclient = v4l2_get_subdevdata(sd);

	/* Default state values */
	state->isp.bad_fw = 0;

	state->preview = NULL;
	state->capture = NULL;

	state->format_mode = V4L2_PIX_FMT_MODE_PREVIEW;
	state->sensor_mode = SENSOR_CAMERA;
	state->flash_mode = FLASH_MODE_OFF;
	state->beauty_mode = 0;

	state->fps = 0;			/* auto */

	memset(&state->focus, 0, sizeof(state->focus));
	gCameraId=val;

#if 1
	//ghm5065 = 1;
	//cam_count = 1;

	//takepicture exceptionally foce quit
	gIs_30W_capture = 1;
	ov2655_enum_framesizes(sd,&cam_frmsize);

	err = OV2655_init_2bytes(sd, (unsigned short **) ov2655_init_reg,OV2655_INIT_REGS);
	if (err < 0) 
	{
		v4l_err(client, "%s: camera initialization failed\n",
				__func__);
		return 0;
	}
#endif
	return 0;
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

static int ov2655_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;
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

static const struct v4l2_subdev_core_ops ov2655_core_ops = {
	.init = ov2655_init,		/* initializing API */
	.queryctrl = ov2655_queryctrl,
	.g_ctrl = ov2655_g_ctrl,
	.g_ext_ctrls = ov2655_g_ext_ctrls,
};

static const struct v4l2_subdev_video_ops ov2655_video_ops = {
	.s_mbus_fmt = ov2655_s_fmt,
	.g_parm = ov2655_g_parm,
	.s_parm = ov2655_s_parm,
	.enum_framesizes = ov2655_enum_framesizes,
	.s_stream = ov2655_s_stream,
};

static const struct v4l2_subdev_ops ov2655_ops = {
	.core = &ov2655_core_ops,
	.video = &ov2655_video_ops,
};

/*
 * ov2655_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int __devinit ov2655_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct ov2655_state *state;
	struct v4l2_subdev *sd;

	state = kzalloc(sizeof(struct ov2655_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	sd = &state->sd;
	strcpy(sd->name, OV2655_DRIVER_NAME);

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &ov2655_ops);

#ifdef CAM_DEBUG
	state->dbg_level = CAM_DEBUG; /*| CAM_TRACE | CAM_I2C;*/
#endif
	/* wait queue initialize */
	init_waitqueue_head(&state->isp.wait);

	return 0;
}

static int __devexit ov2655_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov2655_state *state = to_state(sd);

	if (state->isp.irq > 0)
		free_irq(state->isp.irq, sd);

	v4l2_device_unregister_subdev(sd);

	kfree(state->fw_version);
	kfree(state);

	return 0;
}

static const struct i2c_device_id ov2655_id[] = {
	{ OV2655_DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov2655_id);

static struct i2c_driver ov2655_i2c_driver = {
	.driver = {
		.name	= OV2655_DRIVER_NAME,
	},
	.probe		= ov2655_probe,
	.remove		= __devexit_p(ov2655_remove),
	.id_table	= ov2655_id,
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

MODULE_AUTHOR("www.9tripod.com");
MODULE_DESCRIPTION("9tripod ov2655 2MP camera driver");
MODULE_LICENSE("GPL");
