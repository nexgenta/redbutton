/*
 * MHEGStreamPlayer.c
 */

#include <string.h>
#include <stdio.h>
#include <X11/Xlib.h>

#include "MHEGEngine.h"
#include "MHEGStreamPlayer.h"
#include "MHEGVideoOutput.h"
#include "MHEGAudioOutput.h"
#include "mpegts.h"
#include "utils.h"

/* internal routines */
static void *decode_thread(void *);
static void *video_thread(void *);
static void *audio_thread(void *);

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

LIST_TYPE(AudioFrame) *
new_AudioFrameListItem(void)
{
	LIST_TYPE(AudioFrame) *af = safe_malloc(sizeof(LIST_TYPE(AudioFrame)));

	af->item.pts = AV_NOPTS_VALUE;

	af->item.size = 0;

	return af;
}

void
free_AudioFrameListItem(LIST_TYPE(AudioFrame) *af)
{
	safe_free(af);

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

	p->audio_codec = NULL;

	pthread_mutex_init(&p->videoq_lock, NULL);
	p->videoq = NULL;

	pthread_mutex_init(&p->audioq_lock, NULL);
	p->audioq = NULL;

	return;
}

void
MHEGStreamPlayer_fini(MHEGStreamPlayer *p)
{
	MHEGStreamPlayer_stop(p);

	pthread_mutex_destroy(&p->videoq_lock);
	pthread_mutex_destroy(&p->audioq_lock);

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
	 * we have three threads:
	 * decode_thread reads MPEG data from the TS and decodes it into YUV video frames and audio samples
	 * video_thread takes YUV frames off the videoq list, converts them to RGB and displays them on the screen
	 * audio_thread takes audio samples off the audioq list and feeds them into the sound card
	 */
	if(pthread_create(&p->decode_tid, NULL, decode_thread, p) != 0)
		fatal("Unable to create MPEG decoder thread");

	if(pthread_create(&p->video_tid, NULL, video_thread, p) != 0)
		fatal("Unable to create video output thread");

	if(pthread_create(&p->audio_tid, NULL, audio_thread, p) != 0)
		fatal("Unable to create audio output thread");

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
	pthread_join(p->audio_tid, NULL);

	/* clean up */
	LIST_FREE(&p->videoq, VideoFrame, free_VideoFrameListItem);
	LIST_FREE(&p->audioq, AudioFrame, free_AudioFrameListItem);

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
 * decodes the data into YUV video frames and audio samples
 * adds them to the tail of the videoq and audioq lists
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
	LIST_TYPE(AudioFrame) *audio_frame;
	AudioFrame *af;
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
		verbose("MHEGStreamPlayer: Video: stream type=%d codec=%s", p->video_type, codec->name);
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
		verbose("MHEGStreamPlayer: Audio: stream type=%d codec=%s", p->audio_type, codec->name);
		/* let the audio ouput thread know what the sample rate, etc are */
		p->audio_codec = audio_codec_ctx;
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
		/* see what stream we got a packet for */
		if(pkt.stream_index == p->audio_pid && pkt.pts != AV_NOPTS_VALUE)
		{
			pts = pkt.pts / audio_time_base;
			data = pkt.data;
			size = pkt.size;
			while(size > 0)
			{
				audio_frame = new_AudioFrameListItem();
				af = &audio_frame->item;
				used = avcodec_decode_audio(audio_codec_ctx, af->data, &af->size, data, size);
				data += used;
				size -= used;
				if(af->size > 0)
				{
					af->pts = pts;
					/* 16-bit samples, but af->size is in bytes */
					pts += (af->size / 2.0) / (audio_codec_ctx->channels * audio_codec_ctx->sample_rate);
					pthread_mutex_lock(&p->audioq_lock);
					LIST_APPEND(&p->audioq, audio_frame);
					pthread_mutex_unlock(&p->audioq_lock);
				}
				else
				{
					free_AudioFrameListItem(audio_frame);
				}
			}
			/* don't want one thread hogging the CPU time */
			pthread_yield();
		}
		else if(pkt.stream_index == p->video_pid && pkt.dts != AV_NOPTS_VALUE)
		{
			(void) avcodec_decode_video(video_codec_ctx, frame, &got_picture, pkt.data, pkt.size);
			if(got_picture)
			{
				pts = pkt.dts / video_time_base;
				video_frame = new_VideoFrameListItem(pts, video_codec_ctx->pix_fmt, video_codec_ctx->width, video_codec_ctx->height, frame);
				pthread_mutex_lock(&p->videoq_lock);
				LIST_APPEND(&p->videoq, video_frame);
				pthread_mutex_unlock(&p->videoq_lock);
				/* don't want one thread hogging the CPU time */
				pthread_yield();
			}
		}
		else
		{
			verbose("MHEGStreamPlayer: decoder got unexpected/untimed packet");
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
		verbose("MHEGStreamPlayer: buffered %f seconds of video", buffered);
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
		{
			verbose("MHEGStreamPlayer: dropped frame %u (usecs=%d)", nframes, usecs);
		}
		else
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
 * audio thread
 * takes audio samples of the audioq and feeds them into the sound card as fast as possible
 * MHEGAudioOuput_addSamples() will block while the sound card buffer is full
 */

static void *
audio_thread(void *arg)
{
	MHEGStreamPlayer *p = (MHEGStreamPlayer *) arg;
	MHEGAudioOutput ao;
	AudioFrame *af;
	double buffered;
	double base_pts;
	int64_t base_time;
	snd_pcm_format_t format = 0;	/* keep the compiler happy */
	unsigned int rate;
	unsigned int channels;

	if(!p->have_audio)
		return NULL;

	verbose("MHEGStreamPlayer: audio thread started");

	/* assert */
	if(p->audio == NULL)
		fatal("audio_thread: AudioClass is NULL");

	/* do we need to sync the audio with the video */
#if 0
	if(p->have_video)
	{
		/* wait until the video thread tells us it has some frames buffered up */
/* TODO */
		/* get the video's first PTS and first actual time stamp values */
/* TODO */
/* video thread should set base_pts and base_time from the values for the first frame it displays */
		base_time = p->base_time;
		base_pts = p->base_pts;
	}
	else
#endif
	{
		/* wait until we have some audio frames buffered up */
		do
		{
			pthread_mutex_lock(&p->audioq_lock);
			if(p->audioq != NULL)
				buffered = p->audioq->prev->item.pts - p->audioq->item.pts;
			else
				buffered = 0.0;
			pthread_mutex_unlock(&p->audioq_lock);
			verbose("MHEGStreamPlayer: buffered %f seconds of audio", buffered);
			/* let the decoder have a go */
			if(buffered < INIT_AUDIO_BUFFER_WAIT)
				pthread_yield();
		}
		while(!p->stop && buffered < INIT_AUDIO_BUFFER_WAIT);
		/* the time that we played the first frame */
		base_time = av_gettime();
		pthread_mutex_lock(&p->audioq_lock);
		base_pts = p->audioq->item.pts;
		pthread_mutex_unlock(&p->audioq_lock);
	}

	/* do we need to bomb out early */
	if(p->stop)
		return NULL;

	/* even if this fails, we still need to consume the audioq */
	(void) MHEGAudioOutput_init(&ao);

	/* assert - if audioq is not empty then the codec cannot be NULL */
	if(p->audio_codec == NULL)
		fatal("audio_codec is NULL");

	/* TODO will these be big endian on a big endian machine? */
	if(p->audio_codec->sample_fmt == SAMPLE_FMT_S16)
		format = SND_PCM_FORMAT_S16_LE;
	else if(p->audio_codec->sample_fmt == SAMPLE_FMT_S32)
		format = SND_PCM_FORMAT_S32_LE;
	else
		fatal("Unsupported audio sample format (%d)", p->audio_codec->sample_fmt);

	rate = p->audio_codec->sample_rate;
	channels = p->audio_codec->channels;

	verbose("MHEGStreamPlayer: audio params: format=%d rate=%d channels=%d", format, rate, channels);

/* TODO */
/* hmmm... audio_time_base issue? */
rate=(rate*10)/9;
	(void) MHEGAudioOutput_setParams(&ao, format, rate, channels);

	/* until we are told to stop */
	while(!p->stop)
	{
		/* get the next audio frame */
		pthread_mutex_lock(&p->audioq_lock);
		af = (p->audioq != NULL) ? &p->audioq->item : NULL;
		/* only we delete items from the audioq, so af will stay valid */
		pthread_mutex_unlock(&p->audioq_lock);
		if(af == NULL)
		{
			/* if audio init failed, we will get this a lot */
//			verbose("MHEGStreamPlayer: audioq is empty");
			/* give the decoder a bit of time to catch up */
			pthread_yield();
			continue;
		}
/* TODO */
/* need to make sure pts is what we expect */
/* if we missed decoding a sample, play silence */
		/* this will block until the sound card can take the data */
		MHEGAudioOutput_addSamples(&ao, af->data, af->size);
		/* we can delete the frame from the queue now */
		pthread_mutex_lock(&p->audioq_lock);
		LIST_FREE_HEAD(&p->audioq, AudioFrame, free_AudioFrameListItem);
		pthread_mutex_unlock(&p->audioq_lock);
	}

	MHEGAudioOutput_fini(&ao);

	verbose("MHEGStreamPlayer: audio thread stopped");

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

