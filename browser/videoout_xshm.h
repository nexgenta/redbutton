/*
 * videoout_xshm.h
 */

#ifndef __VIDEOOUT_XSHM_H__
#define __VIDEOOUT_XSHM_H__

#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

typedef struct
{
	unsigned int width;
	unsigned int height;
} FrameSize;

typedef struct
{
	XImage *current_frame;			/* frame we are currently displaying */
	XShmSegmentInfo shm;			/* shared memory used by current_frame */
	AVPicture rgb_frame;			/* ffmpeg wrapper for current_frame SHM data */
	enum PixelFormat out_format;		/* rgb_frame ffmpeg pixel format */
        struct SwsContext *sws_ctx;		/* converts to RGB and resizes if needed */
	FrameSize resize_in;			/* input dimensions */
	FrameSize resize_out;			/* output dimensions */
} vo_xshm_ctx;

extern MHEGVideoOutputMethod vo_xshm_fns;

#endif	/* __VIDEOOUT_XSHM_H__ */
