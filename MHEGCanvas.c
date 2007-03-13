/*
 * MHEGCanvas.c
 */

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#include "ISO13522-MHEG-5.h"
#include "MHEGCanvas.h"
#include "MHEGEngine.h"
#include "utils.h"

/* internal functions */
static unsigned long pixel_value(XRenderPictFormat *, MHEGColour *);

MHEGCanvas *
new_MHEGCanvas(unsigned int width, unsigned int height)
{
	MHEGCanvas *c = safe_mallocz(sizeof(MHEGCanvas));
	MHEGDisplay *d = MHEGEngine_getDisplay();

	/* scale width/height if fullscreen */
	c->width = (width * d->xres) / MHEG_XRES;
	c->height = (height * d->yres) / MHEG_YRES;

	/* no border set yet */
	c->border = 0;

	/* we want a 32-bit RGBA pixel format */
	c->pic_format = XRenderFindStandardFormat(d->dpy, PictStandardARGB32);

	/* create a Pixmap to draw on */
	c->contents = XCreatePixmap(d->dpy, d->win, c->width, c->height, 32);
	/* associate a Picture with it */
	c->contents_pic = XRenderCreatePicture(d->dpy, c->contents, c->pic_format, 0, NULL);

	/* and a Graphics Context */
	c->gc = XCreateGC(d->dpy, c->contents, 0, NULL);

	return c;
}

void
free_MHEGCanvas(MHEGCanvas *c)
{
	MHEGDisplay *d = MHEGEngine_getDisplay();

	/* assert */
	if(c == NULL)
		fatal("free_MHEGCanvas: passed a NULL canvas");

	XRenderFreePicture(d->dpy, c->contents_pic);
	XFreePixmap(d->dpy, c->contents);
	XFreeGC(d->dpy, c->gc);

	safe_free(c);

	return;
}

/*
 * set a border, no drawing will be done in the border (apart from the border itself)
 * width is in pixels (and will be scaled up by this routine in full screen mode)
 * style should be one of:
 * LineStyle_solid
 * LineStyle_dashed
 * LineStyle_dotted
 * (note: UK MHEG Profile says we can treat ALL line styles as solid)
 */

void
MHEGCanvas_setBorder(MHEGCanvas *c, int width, int style, MHEGColour *colour)
{
	MHEGDisplay *d = MHEGEngine_getDisplay();
	XGCValues gcvals;
	XRectangle clip_rect;

	if(width <= 0)
		return;

	if(style != LineStyle_solid)
		error("MHEGCanvas_setBorder: LineStyle %d not supported (using a solid line)", style);

	/* scale width if fullscreen */
	c->border = (width * d->xres) / MHEG_XRES;

	/* draw the border */
	gcvals.foreground = pixel_value(c->pic_format, colour);
	XChangeGC(d->dpy, c->gc, GCForeground, &gcvals);
	/* top */
	XFillRectangle(d->dpy, c->contents, c->gc, 0, 0, c->width, c->border - 1);
	/* bottom */
	XFillRectangle(d->dpy, c->contents, c->gc, 0, (c->height - c->border) - 1, c->width, c->border - 1);
	/* left */
	XFillRectangle(d->dpy, c->contents, c->gc, 0, 0, c->border - 1, c->height);
	/* right */
	XFillRectangle(d->dpy, c->contents, c->gc, (c->width - c->border) - 1, 0, c->border - 1, c->height);

	/* set a clip mask, so no futher drawing will change the border */
	clip_rect.x = c->border;
	clip_rect.y = c->border;
	clip_rect.width = c->width - (2 * c->border);
	clip_rect.height = c->height - (2 * c->border);
	XSetClipRectangles(d->dpy, c->gc, 0, 0, &clip_rect, 1, Unsorted);

	return;
}

/*
 * drawing routine notes:
 * if border width is W, then coord {W,W} is the first pixel that will not be hidden by the border
 * all coords will be scaled up by these drawing routines if we are using full screen mode
 * UK MHEG Profile says we can treat ALL line styles as solid
 * UK MHEG Profile says no alpha blending is done within the DynamicLineArtClass canvas
 * ie all pixel values are put directly onto the canvas, replacing what was there before
 */

/*
 * fill the image (excluding the border) with the given colour
 */

void
MHEGCanvas_clear(MHEGCanvas *c, MHEGColour *colour)
{
/* TODO */
printf("TODO: MHEGCanvas_clear: RGBT=%02x%02x%02x%02x\n", colour->r, colour->g, colour->b, colour->t);

	return;
}

/*
 * convert the MHEGColour to a pixel value
 */

static unsigned long
pixel_value(XRenderPictFormat *format, MHEGColour *colour)
{
	unsigned long pixel;

	/* MHEGColour and PictStandardARGB32 both have 8-bits per RGBA component */
	pixel = colour->r << format->direct.red;
	pixel |= colour->g << format->direct.green;
	pixel |= colour->b << format->direct.blue;
	/* MHEGColour uses transparency, XRender uses opacity */
	pixel |= (255 - colour->t) << format->direct.alpha;

	return pixel;
}

