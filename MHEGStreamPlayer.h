/*
 * MHEGStreamPlayer.h
 */

#ifndef __MHEGSTREAMPLAYER_H__
#define __MHEGSTREAMPLAYER_H__

#include <stdbool.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <ffmpeg/avformat.h>
#include <ffmpeg/avcodec.h>

#include "ISO13522-MHEG-5.h"

/* seconds of video to buffer before we start playing it */
#define INIT_VIDEO_BUFFER_WAIT	1.0

/* list of decoded video frames to be displayed */
typedef struct
{
	double pts;			/* presentation time stamp */
	enum PixelFormat pix_fmt;
	unsigned int width;
	unsigned int height;
	AVPicture frame;
	unsigned char *frame_data;
} VideoFrame;

DEFINE_LIST_OF(VideoFrame);

LIST_TYPE(VideoFrame) *new_VideoFrameListItem(double, enum PixelFormat, unsigned int, unsigned int, AVFrame *);
void free_VideoFrameListItem(LIST_TYPE(VideoFrame) *);

/* player state */
typedef struct
{
	bool playing;				/* true when our threads are active */
	bool stop;				/* true => stop playback */
	bool have_video;			/* false if we have no video stream */
	bool have_audio;			/* false if we have no audio stream */
	int video_tag;				/* video stream component tag (-1 => default for current service ID) */
	int video_pid;				/* PID in MPEG Transport Stream (-1 => not yet known) */
	int video_type;				/* video stream type (-1 => not yet known) */
	int audio_tag;				/* audio stream component tag (-1 => default for current service ID) */
	int audio_pid;				/* PID in MPEG Transport Stream (-1 => not yet known) */
	int audio_type;				/* audio stream type (-1 => not yet known) */
	FILE *ts;				/* MPEG Transport Stream */
	VideoClass *video;			/* output size/position, maybe NULL if audio only */
	pthread_t decode_tid;			/* thread decoding the MPEG stream into frames */
	pthread_t video_tid;			/* thread displaying frames on the screen */
	pthread_mutex_t videoq_lock;		/* list of decoded video frames */
	LIST_OF(VideoFrame) *videoq;		/* head of list is next to be displayed */
	pthread_mutex_t current_frame_lock;	/* locked when we are updating current frame */
	XImage *current_frame;			/* frame we are currently displaying */
} MHEGStreamPlayer;

void MHEGStreamPlayer_init(MHEGStreamPlayer *);
void MHEGStreamPlayer_fini(MHEGStreamPlayer *);

void MHEGStreamPlayer_setVideoTag(MHEGStreamPlayer *, VideoClass *, int);
void MHEGStreamPlayer_setAudioTag(MHEGStreamPlayer *, int);

void MHEGStreamPlayer_play(MHEGStreamPlayer *);
void MHEGStreamPlayer_stop(MHEGStreamPlayer *);

#endif	/* __MHEGSTREAMPLAYER_H__ */
