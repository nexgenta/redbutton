/*
 * MHEGStreamPlayer.c
 */

#include <string.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "MHEGEngine.h"
#include "MHEGStreamPlayer.h"
#include "MHEGVideoOutput.h"
#include "mpegts.h"
#include "utils.h"

/* internal routines */
static void *decode_thread(void *);
static void *video_thread(void *);

static void thread_usleep(unsigned long);
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
	p->audio = NULL;

	pthread_mutex_init(&p->videoq_lock, NULL);
	p->videoq_len = 0;
	p->videoq = NULL;

	return;
}

void
MHEGStreamPlayer_fini(MHEGStreamPlayer *p)
{
	MHEGStreamPlayer_stop(p);

	pthread_mutex_destroy(&p->videoq_lock);

	return;
}

void
MHEGStreamPlayer_setVideoStream(MHEGStreamPlayer *p, VideoClass *video)
{
	if(p->have_video)
		error("MHEGStreamPlayer: more than one video stream; only using the last one (%d)", video->component_tag);

	p->have_video = true;

	/* output size/position */
	p->video = video;

	/* the backend will tell us the PID and stream type when we start streaming it */
	p->video_tag = video->component_tag;
	p->video_pid = -1;
	p->video_type = -1;

	return;
}

void
MHEGStreamPlayer_setAudioStream(MHEGStreamPlayer *p, AudioClass *audio)
{
	if(p->have_audio)
		error("MHEGStreamPlayer: more than one audio stream; only using the last one (%d)", audio->component_tag);

	p->have_audio = true;

	/* volume */
	p->audio = audio;

	/* the backend will tell us the PID and stream type when we start streaming it */
	p->audio_tag = audio->component_tag;
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

//{
// whole machine locks up if you do this
//	struct sched_param sp = {1};
//	if(pthread_setschedparam(p->video_tid, SCHED_RR, &sp) != 0)
//		error("MHEGStreamPlayer: unable to give video thread realtime priority");
//}

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
	p->videoq_len = 0;
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
	double audio_time_base = 90000.0;
	double pts;
	AVFrame *frame;
	LIST_TYPE(VideoFrame) *video_frame;
	int got_picture;
	uint16_t audio_data[AVCODEC_MAX_AUDIO_FRAME_SIZE];
	int audio_size;
	int used;
	unsigned char *data;
	int size;

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
		if(pkt.stream_index == p->audio_pid && pkt.pts != AV_NOPTS_VALUE)
		{
//printf("decode: got audio packet\n");
			pts = pkt.pts;
			data = pkt.data;
			size = pkt.size;
			while(size > 0)
			{
				used = avcodec_decode_audio(audio_codec_ctx, audio_data, &audio_size, data, size);
//printf("decode audio: pts=%f used=%d (size=%d) audio_size=%d\n", pts / audio_time_base, used, size, audio_size);
				data += used;
				size -= used;
				if(audio_size > 0)
				{
					pts += (audio_size * 1000.0) / ((audio_codec_ctx->channels * 2) * audio_codec_ctx->sample_rate);
//					audio_frame = new_AudioFrameListItem(pts / audio_time_base, audio_data, audio_size);
//					pthread_mutex_lock(&opts->audioq_lock);
//					LIST_APPEND(&opts->audioq, audio_frame);
//					pthread_mutex_unlock(&opts->audioq_lock);
				}
			}
			/* don't want one thread hogging the CPU time */
			pthread_yield();
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
				p->videoq_len ++;
				LIST_APPEND(&p->videoq, video_frame);
//printf("decode_thread: add frame: len=%u\n", p->videoq_len);
				pthread_mutex_unlock(&p->videoq_lock);
//printf("decode: got video frame: pts=%f (real pts=%f) width=%d height=%d\n", pts, pkt.pts / video_time_base, video_codec_ctx->width, video_codec_ctx->height);
				/* don't want one thread hogging the CPU time */
				pthread_yield();
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
	MHEGVideoOutput vo;
	int out_x;
	int out_y;
	unsigned int out_width;
	unsigned int out_height;
	VideoFrame *vf;
	double buffered;
	double last_pts;
	int64_t last_time, this_time, now;
	int usecs;
	bool drop_frame;
	unsigned int nframes = 0;

	if(!p->have_video)
		return NULL;

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

	/* initialise the video output method */
	MHEGVideoOutput_init(&vo);

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
			/* scale the next frame if necessary */
			pthread_mutex_lock(&p->video->inst.scaled_lock);
			/* use scaled values if ScaleVideo has been called */
			if(p->video->inst.scaled)
			{
				out_width = p->video->inst.scaled_width;
				out_height = p->video->inst.scaled_height;
			}
			else
			{
				out_width = vf->width;
				out_height = vf->height;
			}
			pthread_mutex_unlock(&p->video->inst.scaled_lock);
			/* scale up if fullscreen */
			if(d->fullscreen)
			{
				out_width = (out_width * d->xres) / MHEG_XRES;
				out_height = (out_height * d->yres) / MHEG_YRES;
			}
			MHEGVideoOutput_prepareFrame(&vo, vf, out_width, out_height);
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
//printf("display frame %d: pts=%f this_time=%lld real_time=%lld (diff=%lld)\n", nframes, vf->pts, last_time, now, now-last_time);
			/* origin of VideoClass */
			pthread_mutex_lock(&p->video->inst.bbox_lock);
			out_x = p->video->inst.Position.x_position;
			out_y = p->video->inst.Position.y_position;
			pthread_mutex_unlock(&p->video->inst.bbox_lock);
			/* scale if fullscreen */
			if(d->fullscreen)
			{
				out_x = (out_x * d->xres) / MHEG_XRES;
				out_y = (out_y * d->yres) / MHEG_YRES;
			}
			/* draw the current frame */
			MHEGVideoOutput_drawFrame(&vo, out_x, out_y);
			/* redraw objects above the video */
			pthread_mutex_lock(&p->video->inst.bbox_lock);
			MHEGDisplay_refresh(d, &p->video->inst.Position, &p->video->inst.BoxSize);
			pthread_mutex_unlock(&p->video->inst.bbox_lock);
			/* get it drawn straight away */
			XFlush(d->dpy);
		}
		/* we can delete the frame from the queue now */
		pthread_mutex_lock(&p->videoq_lock);
//printf("video_thread: free frame: len=%u\n", p->videoq_len);
		/* assert */
		if(p->videoq_len == 0)
			fatal("video_thread: videoq_len is 0");
		p->videoq_len --;
		LIST_FREE_HEAD(&p->videoq, VideoFrame, free_VideoFrameListItem);
		pthread_mutex_unlock(&p->videoq_lock);
		/* don't want one thread hogging the CPU time */
		pthread_yield();
	}

	MHEGVideoOutput_fini(&vo);

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

