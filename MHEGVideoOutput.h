/*
 * MHEGVideoOutput.h
 */

#ifndef __MHEGVIDEOOUTPUT_H__
#define __MHEGVIDEOOUTPUT_H__

typedef struct
{
	XShmSegmentInfo shm;
	AVPicture rgb_frame;
	enum PixelFormat out_format;
	ImgReSampleContext *resize_ctx;
	AVPicture resized_frame;
	uint8_t *tmpbuf_data;
	pthread_mutex_t current_frame_lock;	/* locked when we are updating current frame */
	XImage *current_frame;			/* frame we are currently displaying */
} MHEGVideoOutput;

void MHEGVideoOutput_init(MHEGVideoOutput *);
void MHEGVideoOutput_fini(MHEGVideoOutput *);

void MHEGVideoOutput_prepareFrame(MHEGVideoOutput *, VideoFrame *, unsigned int, unsigned int);
void MHEGVideoOutput_drawFrame(MHEGVideoOutput *, int, int);

#endif 	/* __MHEGVIDEOOUTPUT_H__ */

