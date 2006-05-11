/*
 * MHEGStreamPlayer.c
 */

#include <string.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "MHEGEngine.h"
#include "MHEGStreamPlayer.h"
#include "mpegts.h"
#include "utils.h"

/* internal routines */
static void *decode_thread(void *);
static void *video_thread(void *);

static void thread_usleep(unsigned long);
static enum PixelFormat find_av_pix_fmt(int, unsigned long, unsigned long, unsigned long);
static enum CodecID find_av_codec_id(int);

LIST_TYPE(VideoFrame) *
new_VideoFrameListItem(double pts, enum PixelFormat pix_fmt, unsigned int width, unsigned int height, AVFrame *frame)
{
	LIST_TYPE(VideoFrame) *vf = safe_malloc(sizeof(LIST_TYPE(VideoFrame)));
	int frame_size;

	vf->item.pts = pts;
	vf->item.pix_fmt = pix_fmt;
	vf->item.width = width;
	vf->item.height = height;

	/*
	 * take a copy of the frame,
	 * the actual data is inside the video codec somewhere and will be overwritten by the next frame we decode
	 */
	if((frame_size = avpicture_get_size(pix_fmt, width, height)) < 0)
		fatal("Invalid frame_size");
	vf->item.frame_data = safe_malloc(frame_size);
	avpicture_fill(&vf->item.frame, vf->item.frame_data, pix_fmt, width, height);
	img_copy(&vf->item.frame, (AVPicture*) frame, pix_fmt, width, height);

	return vf;
}

void
free_VideoFrameListItem(LIST_TYPE(VideoFrame) *vf)
{
	safe_free(vf->item.frame_data);

	safe_free(vf);

	return;
}

void
MHEGStreamPlayer_init(MHEGStreamPlayer *p)
{
	bzero(p, sizeof(MHEGStreamPlayer));

	p->playing = false;
	p->stop = false;

	p->have_video = false;
	p->have_audio = false;

	p->video = NULL;

	pthread_mutex_init(&p->videoq_lock, NULL);
	p->videoq = NULL;

	pthread_mutex_init(&p->current_frame_lock, NULL);

	return;
}

void
MHEGStreamPlayer_fini(MHEGStreamPlayer *p)
{
	MHEGStreamPlayer_stop(p);

	pthread_mutex_destroy(&p->videoq_lock);
	pthread_mutex_destroy(&p->current_frame_lock);

	return;
}

void
MHEGStreamPlayer_setVideoTag(MHEGStreamPlayer *p, VideoClass *video, int tag)
{
	if(p->have_video)
		error("MHEGStreamPlayer: more than one video stream; only using the last one (%d)", tag);

	p->have_video = true;
	p->video_tag = tag;
	p->video_pid = -1;
	p->video_type = -1;

	/* output size/position */
	p->video = video;

	return;
}

void
MHEGStreamPlayer_setAudioTag(MHEGStreamPlayer *p, int tag)
{
	if(p->have_audio)
		error("MHEGStreamPlayer: more than one audio stream; only using the last one (%d)", tag);

	p->have_audio = true;
	p->audio_tag = tag;
	p->audio_pid = -1;
	p->audio_type = -1;

	return;
}

void
MHEGStreamPlayer_play(MHEGStreamPlayer *p)
{
	verbose("MHEGStreamPlayer_play: audio_tag=%d video_tag=%d", p->audio_tag, p->video_tag);

	p->audio_pid = p->audio_tag;
	p->video_pid = p->video_tag;
	if((p->ts = MHEGEngine_openStream(p->have_audio, &p->audio_pid, &p->audio_type,
					  p->have_video, &p->video_pid, &p->video_type)) == NULL)
	{
		error("Unable to open MPEG stream");
		return;
	}

	p->playing = true;
	p->stop = false;

	/*
	 * we have two threads:
	 * decode_thread reads MPEG data from the TS and decodes it into YUV video frames
	 * video_thread takes YUV frames off the videoq list, converts them to RGB and displays them on the screen
	 */
	if(pthread_create(&p->decode_tid, NULL, decode_thread, p) != 0)
		fatal("Unable to create MPEG decoder thread");

	if(pthread_create(&p->video_tid, NULL, video_thread, p) != 0)
		fatal("Unable to create video output thread");

	return;
}

void
MHEGStreamPlayer_stop(MHEGStreamPlayer *p)
{
	verbose("MHEGStreamPlayer_stop");

	/* are we playing */
	if(!p->playing)
		return;

	/* signal the threads to stop */
	p->stop = true;

	/* wait for them to finish */
	pthread_join(p->decode_tid, NULL);
	pthread_join(p->video_tid, NULL);

	/* clean up */
	LIST_FREE(&p->videoq, VideoFrame, free_VideoFrameListItem);

	if(p->ts != NULL)
	{
		fclose(p->ts);
		p->ts = NULL;
	}

	p->playing = false;

	return;
}

/*
 * decode_thread
 * reads the MPEG TS file
 * decodes the data into YUV video frames
 * adds them to the tail of the videoq list
 */

static void *
decode_thread(void *arg)
{
	MHEGStreamPlayer *p = (MHEGStreamPlayer *) arg;
	MpegTSContext *tsdemux;
	AVPacket pkt;
	AVCodecContext *audio_codec_ctx = NULL;
	AVCodecContext *video_codec_ctx = NULL;
	enum CodecID codec_id;
	AVCodec *codec = NULL;
	double video_time_base = 90000.0;
	double pts;
	AVFrame *frame;
	LIST_TYPE(VideoFrame) *video_frame;
	int got_picture;

	verbose("MHEGStreamPlayer: decode thread started");

	if(p->video_pid != -1)
	{
		if((video_codec_ctx = avcodec_alloc_context()) == NULL)
			fatal("Out of memory");
		if((codec_id = find_av_codec_id(p->video_type)) == CODEC_ID_NONE
		|| (codec = avcodec_find_decoder(codec_id)) == NULL)
			fatal("Unsupported video codec");
		if(avcodec_open(video_codec_ctx, codec) < 0)
			fatal("Unable to open video codec");
		verbose("MHEGStreamPlayer: Video: stream type=%d codec=%s\n", p->video_type, codec->name);
	}

	if(p->audio_pid != -1)
	{
		if((audio_codec_ctx = avcodec_alloc_context()) == NULL)
			fatal("Out of memory");
		if((codec_id = find_av_codec_id(p->audio_type)) == CODEC_ID_NONE
		|| (codec = avcodec_find_decoder(codec_id)) == NULL)
			fatal("Unsupported audio codec");
		if(avcodec_open(audio_codec_ctx, codec) < 0)
			fatal("Unable to open audio codec");
		verbose("MHEGStreamPlayer: Audio: stream type=%d codec=%s\n", p->audio_type, codec->name);
	}

	if((frame = avcodec_alloc_frame()) == NULL)
		fatal("Out of memory");

	if((tsdemux = mpegts_open(p->ts)) == NULL)
		fatal("Out of memory");

	while(!p->stop && !feof(p->ts))
	{
		/* get the next complete packet for one of the streams */
		if(mpegts_demux_frame(tsdemux, &pkt) < 0)
			continue;
//if(pkt.dts == AV_NOPTS_VALUE) printf("NO DTS on PID %d!\n", pkt.stream_index);
//if(pkt.pts == AV_NOPTS_VALUE) printf("NO PTS on PID %d!\n", pkt.stream_index);
//printf("PTS=%lld DTS=%lld\n", pkt.pts, pkt.dts);
		/* see what stream we got a packet for */
		if(pkt.stream_index == p->audio_pid && pkt.dts != AV_NOPTS_VALUE)
		{
//printf("decode: got audio packet\n");
		}
		else if(pkt.stream_index == p->video_pid && pkt.dts != AV_NOPTS_VALUE)
		{
//printf("decode: got video packet\n");
			(void) avcodec_decode_video(video_codec_ctx, frame, &got_picture, pkt.data, pkt.size);
			if(got_picture)
			{
				pts = pkt.dts / video_time_base;
				video_frame = new_VideoFrameListItem(pts, video_codec_ctx->pix_fmt, video_codec_ctx->width, video_codec_ctx->height, frame);
				pthread_mutex_lock(&p->videoq_lock);
				LIST_APPEND(&p->videoq, video_frame);
				pthread_mutex_unlock(&p->videoq_lock);
//printf("decode: got video frame: pts=%f (real pts=%f) width=%d height=%d\n", pts, pkt.pts / video_time_base, video_codec_ctx->width, video_codec_ctx->height);
			}
		}
		else
		{
//printf("decode: got unknown/untimed packet\n");
		}
		av_free_packet(&pkt);
	}

	/* clean up */
	mpegts_close(tsdemux);

	av_free(frame);

	if(video_codec_ctx != NULL)
		avcodec_close(video_codec_ctx);
	if(audio_codec_ctx != NULL)
		avcodec_close(audio_codec_ctx);

	verbose("MHEGStreamPlayer: decode thread stopped");

	return NULL;
}

/*
 * video_thread
 * takes YUV frames off the videoq list
 * scales them (if necessary) to fit the output size
 * converts them to RGB
 * waits for the correct time, then displays them on the screen
 */

static void *
video_thread(void *arg)
{
	MHEGStreamPlayer *p = (MHEGStreamPlayer *) arg;
	MHEGDisplay *d = MHEGEngine_getDisplay();
	unsigned int out_width = 0;	/* keep the compiler happy */
	unsigned int out_height = 0;
	VideoFrame *vf;
	AVPicture rgb_frame;
	int rgb_size;
	XShmSegmentInfo shm;
	enum PixelFormat out_format;
	double buffered;
	double last_pts;
	int64_t last_time, this_time, now;
	int usecs;
	bool drop_frame;
	ImgReSampleContext *resize_ctx = NULL;
	AVPicture resized_frame;
	AVPicture *yuv_frame;
	int tmpbuf_size;
	uint8_t *tmpbuf_data = NULL;
	unsigned int nframes = 0;

	verbose("MHEGStreamPlayer: video thread started");

	/* assert */
	if(p->video == NULL)
		fatal("video_thread: VideoClass is NULL");

	/* wait until we have some frames buffered up */
	do
	{
		pthread_mutex_lock(&p->videoq_lock);
		if(p->videoq != NULL)
			buffered = p->videoq->prev->item.pts - p->videoq->item.pts;
		else
			buffered = 0.0;
		pthread_mutex_unlock(&p->videoq_lock);
		verbose("MHEGStreamPlayer: buffered %f seconds", buffered);
		/* let the decoder have a go */
		if(buffered < INIT_VIDEO_BUFFER_WAIT)
			pthread_yield();
	}
	while(!p->stop && buffered < INIT_VIDEO_BUFFER_WAIT);

	/* do we need to bomb out early */
	if(p->stop)
		return NULL;

	/* assert */
	if(p->videoq == NULL)
		fatal("video_thread: no frames!");

	/* size of first frame, no need to lock as item won't change */
	out_width = p->videoq->item.width;
	out_height = p->videoq->item.height;
/* TODO */
/* also scale if ScaleVideo has been called */
	if(d->fullscreen)
	{
		out_width = (out_width * d->xres) / MHEG_XRES;
		out_height = (out_height * d->yres) / MHEG_YRES;
	}

	if((p->current_frame = XShmCreateImage(d->dpy, d->vis, d->depth, ZPixmap, NULL, &shm, out_width, out_height)) == NULL)
		fatal("XShmCreateImage failed");

	/* work out what ffmpeg pixel format matches our XImage format */
	if((out_format = find_av_pix_fmt(p->current_frame->bits_per_pixel,
					 d->vis->red_mask, d->vis->green_mask, d->vis->blue_mask)) == PIX_FMT_NONE)
		fatal("Unsupported XImage pixel format");

	rgb_size = p->current_frame->bytes_per_line * out_height;

	if(rgb_size != avpicture_get_size(out_format, out_width, out_height))
		fatal("XImage and ffmpeg pixel formats differ");

	if((shm.shmid = shmget(IPC_PRIVATE, rgb_size, IPC_CREAT | 0777)) == -1)
		fatal("shmget failed");
	if((shm.shmaddr = shmat(shm.shmid, NULL, 0)) == (void *) -1)
		fatal("shmat failed");
	shm.readOnly = True;
	if(!XShmAttach(d->dpy, &shm))
		fatal("XShmAttach failed");

	/* we made sure these pixel formats are the same */
	p->current_frame->data = shm.shmaddr;
	avpicture_fill(&rgb_frame, shm.shmaddr, out_format, out_width, out_height);

	/* the time that we displayed the previous frame */
	last_time = 0;
	last_pts = 0;

	/* until we are told to stop... */
	while(!p->stop)
	{
		/* get the next frame */
		pthread_mutex_lock(&p->videoq_lock);
		vf = (p->videoq != NULL) ? &p->videoq->item : NULL;
		/* only we delete items from the videoq, so vf will stay valid */
		pthread_mutex_unlock(&p->videoq_lock);
		if(vf == NULL)
		{
			verbose("MHEGStreamPlayer: videoq is empty");
			/* give the decoder a bit of time to catch up */
			pthread_yield();
			continue;
		}
		/*
		 * keep track of how many frames we've played
		 * just so the dropped frame verbose message below is more useful
		 * don't care if it wraps back to 0 again
		 */
		nframes ++;
		/* see if we should drop this frame or not */
		now = av_gettime();
		/* work out when this frame should be displayed based on when the last one was */
		if(last_time != 0)
			this_time = last_time + ((vf->pts - last_pts) * 1000000.0);
		else
			this_time = now;
		/* how many usecs do we need to wait */
		usecs = this_time - now;
		/*
		 * we've still got to convert it to RGB and maybe scale it too
		 * so don't bother allowing any error here
		 */
		drop_frame = (usecs < 0);
		if(drop_frame)
			verbose("MHEGStreamPlayer: dropped frame %u (usecs=%d)", nframes, usecs);
//if(drop_frame)
//printf("dropped frame %d: pts=%f last_pts=%f last_time=%lld this_time=%lld usecs=%d\n", nframes, vf->pts, last_pts, last_time, this_time, usecs);
		if(!drop_frame)
		{
			/* see if the output size has changed */
/* TODO */
			/* scale the next frame if necessary */
			if(vf->width != out_width || vf->height != out_height)
			{
/* TODO */
/* need to change resize_ctx if vf->width or vf->height have changed since last time */
/* dont forget: img_resample_close(resize_ctx); */
/* and to free or realloc tmpbuf_data */
				if(resize_ctx == NULL)
				{
					resize_ctx = img_resample_init(out_width, out_height, vf->width, vf->height);
					tmpbuf_size = avpicture_get_size(vf->pix_fmt, out_width, out_height);
					tmpbuf_data = safe_malloc(tmpbuf_size);
					avpicture_fill(&resized_frame, tmpbuf_data, vf->pix_fmt, out_width, out_height);
				}
				img_resample(resize_ctx, &resized_frame, &vf->frame);
				yuv_frame = &resized_frame;
			}
			else
			{
				yuv_frame = &vf->frame;
			}
			/* make sure no-one else is using the RGB frame */
			pthread_mutex_lock(&p->current_frame_lock);
			/* convert the next frame to RGB */
			img_convert(&rgb_frame, out_format, yuv_frame, vf->pix_fmt, out_width, out_height);
/* TODO */
/* overlay any MHEG objects above it */
			/* we've finished changing the RGB frame now */
			pthread_mutex_unlock(&p->current_frame_lock);
			/* wait until it's time to display the frame */
			now = av_gettime();
			/* don't wait if this is the first frame */
			if(last_time != 0)
			{
				/* how many usecs do we need to wait */
				usecs = this_time - now;
//printf("last_time=%lld this_time=%lld now=%lld sleep=%d\n", last_time, this_time, now, usecs);
				if(usecs > 0)
					thread_usleep(usecs);
				/* remember when we should have displayed this frame */
				last_time = this_time;
			}
			else
			{
				/* remember when we displayed this frame */
				last_time = now;
			}
			/* remember the time stamp for this frame */
			last_pts = vf->pts;
//now=av_gettime();
//printf("display frame %d: pts=%f this_time=%lld real_time=%lld (diff=%lld)\n", ++nframes, vf->pts, last_time, now, now-last_time);
/* TODO */
/* take p->video size/position into account */
/* redraw objects above the video */
// disable output until we can overlay the MHEG objects
//			XShmPutImage(d->dpy, d->win, d->win_gc, p->current_frame, 0, 0, 0, 0, out_width, out_height, False);
			/* get it drawn straight away */
			XFlush(d->dpy);
		}
		/* we can delete the frame from the queue now */
		pthread_mutex_lock(&p->videoq_lock);
		LIST_FREE_HEAD(&p->videoq, VideoFrame, free_VideoFrameListItem);
		pthread_mutex_unlock(&p->videoq_lock);
	}

	if(resize_ctx != NULL)
	{
		img_resample_close(resize_ctx);
		safe_free(tmpbuf_data);
	}

	/* the XImage data is our shared memory, make sure XDestroyImage doesn't try to free it */
	p->current_frame->data = NULL;
	XDestroyImage(p->current_frame);

	XShmDetach(d->dpy, &shm);
	shmdt(shm.shmaddr);
	shmctl(shm.shmid, IPC_RMID, NULL);

	verbose("MHEGStreamPlayer: video thread stopped");

	return NULL;
}

/*
 * usleep(usecs)
 * need to make sure the other threads get a go while we are sleeping
 */

static void
thread_usleep(unsigned long usecs)
{
	struct timespec ts;

	ts.tv_sec = (usecs / 1000000);
	ts.tv_nsec = (usecs % 1000000) * 1000;

	nanosleep(&ts, NULL);

	return;
}

/*
 * returns a PIX_FMT_xxx type that matches the given bits per pixel and RGB bit mask values
 * returns PIX_FMT_NONE if none match
 */

static enum PixelFormat
find_av_pix_fmt(int bpp, unsigned long rmask, unsigned long gmask, unsigned long bmask)
{
	enum PixelFormat fmt;

	fmt = PIX_FMT_NONE;
	switch(bpp)
	{
	case 32:
		if(rmask == 0xff0000 && gmask == 0xff00 && bmask == 0xff)
			fmt = PIX_FMT_RGBA32;
		break;

	case 24:
		if(rmask == 0xff0000 && gmask == 0xff00 && bmask == 0xff)
			fmt = PIX_FMT_RGB24;
		else if(rmask == 0xff && gmask == 0xff00 && bmask == 0xff0000)
			fmt = PIX_FMT_BGR24;
		break;

	case 16:
		if(rmask == 0xf800 && gmask == 0x07e0 && bmask == 0x001f)
			fmt = PIX_FMT_RGB565;
		else if(rmask == 0x7c00 && gmask == 0x03e0 && bmask == 0x001f)
			fmt = PIX_FMT_RGB555;
		break;

	default:
		break;
	}

	if(fmt == PIX_FMT_NONE)
		error("Unsupported pixel format (bpp=%d r=%lx g=%lx b=%lx)", bpp, rmask, gmask, bmask);

	return fmt;
}

/*
 * from libavformat/mpegts.c
 */

static enum CodecID
find_av_codec_id(int stream_type)
{
	enum CodecID codec_id;

	codec_id = CODEC_ID_NONE;
	switch(stream_type)
	{
	case STREAM_TYPE_AUDIO_MPEG1:
	case STREAM_TYPE_AUDIO_MPEG2:
		codec_id = CODEC_ID_MP3;
		break;

	case STREAM_TYPE_VIDEO_MPEG1:
	case STREAM_TYPE_VIDEO_MPEG2:
		codec_id = CODEC_ID_MPEG2VIDEO;
		break;

	case STREAM_TYPE_VIDEO_MPEG4:
		codec_id = CODEC_ID_MPEG4;
		break;

	case STREAM_TYPE_VIDEO_H264:
		codec_id = CODEC_ID_H264;
		break;

	case STREAM_TYPE_AUDIO_AAC:
		codec_id = CODEC_ID_AAC;
		break;

	case STREAM_TYPE_AUDIO_AC3:
		codec_id = CODEC_ID_AC3;
		break;

	case STREAM_TYPE_AUDIO_DTS:
		codec_id = CODEC_ID_DTS;
		break;

	default:
		break;
	}

	if(codec_id == CODEC_ID_NONE)
		error("Unsupported audio/video codec (MPEG stream type=%d)", stream_type);

	return codec_id;
}

