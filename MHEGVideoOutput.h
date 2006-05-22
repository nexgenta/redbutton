/*
 * MHEGVideoOutput.h
 */

#ifndef __MHEGVIDEOOUTPUT_H__
#define __MHEGVIDEOOUTPUT_H__

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
	ImgReSampleContext *resize_ctx;		/* NULL if we do not need to resize the frame */
	FrameSize resize_in;			/* resize_ctx input dimensions */
	FrameSize resize_out;			/* resize_ctx output dimensions */
	AVPicture resized_frame;		/* resized output frame */
	uint8_t *resized_data;			/* resized_frame data buffer */
} MHEGVideoOutput;

void MHEGVideoOutput_init(MHEGVideoOutput *);
void MHEGVideoOutput_fini(MHEGVideoOutput *);

void MHEGVideoOutput_prepareFrame(MHEGVideoOutput *, VideoFrame *, unsigned int, unsigned int);
void MHEGVideoOutput_drawFrame(MHEGVideoOutput *, int, int);

#endif 	/* __MHEGVIDEOOUTPUT_H__ */

