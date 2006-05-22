/*
 * MHEGVideoOutput.h
 */

#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <ffmpeg/avformat.h>

#include "MHEGEngine.h"
#include "MHEGVideoOutput.h"
#include "utils.h"

void x11_shm_init(MHEGVideoOutput *);
void x11_shm_fini(MHEGVideoOutput *);
void x11_shm_prepareFrame(MHEGVideoOutput *, VideoFrame *, unsigned int, unsigned int);
void x11_shm_drawFrame(MHEGVideoOutput *, int, int);

static void x11_shm_create_frame(MHEGVideoOutput *, unsigned int, unsigned int);
static void x11_shm_destroy_frame(MHEGVideoOutput *);

static enum PixelFormat find_av_pix_fmt(int, unsigned long, unsigned long, unsigned long);

void
MHEGVideoOutput_init(MHEGVideoOutput *v)
{
	return x11_shm_init(v);
}

void
MHEGVideoOutput_fini(MHEGVideoOutput *v)
{
	return x11_shm_fini(v);
}

void
MHEGVideoOutput_prepareFrame(MHEGVideoOutput *v, VideoFrame *f, unsigned int out_width, unsigned int out_height)
{
	return x11_shm_prepareFrame(v, f, out_width, out_height);
}

void
MHEGVideoOutput_drawFrame(MHEGVideoOutput *v, int x, int y)
{
	return x11_shm_drawFrame(v, x, y);
}

void
x11_shm_init(MHEGVideoOutput *v)
{
	pthread_mutex_init(&v->current_frame_lock, NULL);
	v->current_frame = NULL;

	v->resize_ctx = NULL;

	v->tmpbuf_data = NULL;

	return;
}

void
x11_shm_fini(MHEGVideoOutput *v)
{
	if(v->resize_ctx != NULL)
	{
		img_resample_close(v->resize_ctx);
		safe_free(v->tmpbuf_data);
	}

	if(v->current_frame != NULL)
		x11_shm_destroy_frame(v);

	pthread_mutex_destroy(&v->current_frame_lock);

	return;
}

void
x11_shm_prepareFrame(MHEGVideoOutput *v, VideoFrame *f, unsigned int out_width, unsigned int out_height)
{
	AVPicture resized_frame;
	AVPicture *yuv_frame;
	int tmpbuf_size;

	/* see if the output size has changed since the last frame */
/* TODO if it has, delete the old one or just resize shm etc */
/* will also need to delete resize_ctx */

	if(v->current_frame == NULL)
		x11_shm_create_frame(v, out_width, out_height);

	/* see if the input size is different than the output size */
	if(f->width != out_width || f->height != out_height)
	{
/* TODO */
/* need to change resize_ctx if vf->width or vf->height have changed since last time */
/* dont forget: img_resample_close(resize_ctx); */
/* and to free or realloc tmpbuf_data */
		if(v->resize_ctx == NULL)
		{
			v->resize_ctx = img_resample_init(out_width, out_height, f->width, f->height);
			tmpbuf_size = avpicture_get_size(f->pix_fmt, out_width, out_height);
			v->tmpbuf_data = safe_malloc(tmpbuf_size);
			avpicture_fill(&resized_frame, v->tmpbuf_data, f->pix_fmt, out_width, out_height);
		}
		img_resample(v->resize_ctx, &resized_frame, &f->frame);
		yuv_frame = &resized_frame;
	}
	else
	{
		yuv_frame = &f->frame;
	}

	/* convert the frame to RGB */
	pthread_mutex_lock(&v->current_frame_lock);
	img_convert(&v->rgb_frame, v->out_format, yuv_frame, f->pix_fmt, out_width, out_height);
	pthread_mutex_unlock(&v->current_frame_lock);

	return;
}

void
x11_shm_drawFrame(MHEGVideoOutput *v, int x, int y)
{
	MHEGDisplay *d = MHEGEngine_getDisplay();
	unsigned int out_width;
	unsigned int out_height;

/* TODO */
/* probably dont need this lock anymore, only we use v->current_frame */
	pthread_mutex_lock(&v->current_frame_lock);
	if(v->current_frame != NULL)
	{
		/* video frame is already scaled as needed */
		out_width = v->current_frame->width;
		out_height = v->current_frame->height;
		/* draw it onto the Window contents Pixmap */
		XShmPutImage(d->dpy, d->contents, d->win_gc, v->current_frame, 0, 0, x, y, out_width, out_height, False);
		/* get it drawn straight away */
		XFlush(d->dpy);
	}
	pthread_mutex_unlock(&v->current_frame_lock);

	return;
}

static void
x11_shm_create_frame(MHEGVideoOutput *v, unsigned int out_width, unsigned int out_height)
{
	MHEGDisplay *d = MHEGEngine_getDisplay();
	int rgb_size;

	if((v->current_frame = XShmCreateImage(d->dpy, d->vis, d->depth, ZPixmap, NULL, &v->shm, out_width, out_height)) == NULL)
		fatal("XShmCreateImage failed");

	/* work out what ffmpeg pixel format matches our XImage format */
	if((v->out_format = find_av_pix_fmt(v->current_frame->bits_per_pixel,
					    d->vis->red_mask, d->vis->green_mask, d->vis->blue_mask)) == PIX_FMT_NONE)
		fatal("Unsupported XImage pixel format");

	rgb_size = v->current_frame->bytes_per_line * out_height;

	if(rgb_size != avpicture_get_size(v->out_format, out_width, out_height))
		fatal("XImage and ffmpeg pixel formats differ");

	if((v->shm.shmid = shmget(IPC_PRIVATE, rgb_size, IPC_CREAT | 0777)) == -1)
		fatal("shmget failed");
	if((v->shm.shmaddr = shmat(v->shm.shmid, NULL, 0)) == (void *) -1)
		fatal("shmat failed");
	v->shm.readOnly = True;
	if(!XShmAttach(d->dpy, &v->shm))
		fatal("XShmAttach failed");

	/* we made sure these pixel formats are the same */
	v->current_frame->data = v->shm.shmaddr;
	avpicture_fill(&v->rgb_frame, v->shm.shmaddr, v->out_format, out_width, out_height);

	return;
}

static void
x11_shm_destroy_frame(MHEGVideoOutput *v)
{
	MHEGDisplay *d = MHEGEngine_getDisplay();

	/* get rid of the current frame */
	pthread_mutex_lock(&v->current_frame_lock);
	/* the XImage data is our shared memory, make sure XDestroyImage doesn't try to free it */
	v->current_frame->data = NULL;
	XDestroyImage(v->current_frame);
	/* make sure no-one tries to use it */
	v->current_frame = NULL;
	pthread_mutex_unlock(&v->current_frame_lock);

	XShmDetach(d->dpy, &v->shm);
	shmdt(v->shm.shmaddr);
	shmctl(v->shm.shmid, IPC_RMID, NULL);

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

