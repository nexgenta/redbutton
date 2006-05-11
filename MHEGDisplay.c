/*
 * MHEGDisplay.c
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <png.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/keysym.h>
#include <mpeg2dec/mpeg2.h>
#include <mpeg2dec/mpeg2convert.h>
#include <ffmpeg/avformat.h>

#include "MHEGEngine.h"
#include "MHEGDisplay.h"
#include "readpng.h"
#include "utils.h"

/* internal utils */
static MHEGKeyMapEntry *load_keymap(char *);

static void display_colour(XRenderColor *, MHEGColour *);

/* from GDK MwmUtils.h */
#define MWM_HINTS_DECORATIONS	(1L << 1)
typedef struct
{
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long input_mode;
	unsigned long status;
} MotifWmHints;

/* default keyboard mapping */
static MHEGKeyMapEntry default_keymap[] =
{
	{ XK_Up, MHEGKey_Up },
	{ XK_Down, MHEGKey_Down },
	{ XK_Left, MHEGKey_Left },
	{ XK_Right, MHEGKey_Right },
	{ XK_0, MHEGKey_0 },
	{ XK_1, MHEGKey_1 },
	{ XK_2, MHEGKey_2 },
	{ XK_3, MHEGKey_3 },
	{ XK_4, MHEGKey_4 },
	{ XK_5, MHEGKey_5 },
	{ XK_6, MHEGKey_6 },
	{ XK_7, MHEGKey_7 },
	{ XK_8, MHEGKey_8 },
	{ XK_9, MHEGKey_9 },
	{ XK_Return, MHEGKey_Select },
	{ XK_Escape, MHEGKey_Cancel },
	{ XK_r, MHEGKey_Red },
	{ XK_g, MHEGKey_Green },
	{ XK_y, MHEGKey_Yellow },
	{ XK_b, MHEGKey_Blue },
	{ XK_t, MHEGKey_Text },
	{ 0, 0 }			/* terminator */
};

void
MHEGDisplay_init(MHEGDisplay *d, bool fullscreen, char *keymap)
{
	unsigned int xrender_major;
	unsigned int xrender_minor;
	int x, y;
	XVisualInfo visinfo;
	unsigned long mask;
	XSetWindowAttributes attr;
	Atom wm_delete_window;
	XSizeHints hint;
	unsigned long gcmask;
	XGCValues gcvals;
	XRenderPictFormat *pic_format;
	XRenderPictureAttributes pa;
	Pixmap textfg;
	/* fake argc, argv for XtDisplayInitialize */
	int argc = 0;
	char *argv[1] = { NULL };

	/* remember if we are using fullscreen mode */
	d->fullscreen = fullscreen;

	/* keyboard mapping */
	if(keymap != NULL)
		d->keymap = load_keymap(keymap);
	else
		d->keymap = default_keymap;

	/* so X requests/replies in different threads don't get interleaved */
	XInitThreads();

	if((d->dpy = XOpenDisplay(NULL)) == NULL)
		fatal("Unable to open display");

	/* check the X server supports at least XRender 0.6 (needed for Bilinear filter) */
	xrender_major = 0;
	xrender_minor = 10;
	if(!XRenderQueryVersion(d->dpy, &xrender_major, &xrender_minor)
	|| xrender_minor < 6)
		fatal("X Server does not support XRender 0.6 or above");

	/* size of the Window */
	if(fullscreen)
	{
		d->xres = WidthOfScreen(DefaultScreenOfDisplay(d->dpy));
		d->yres = HeightOfScreen(DefaultScreenOfDisplay(d->dpy));
	}
	else
	{
		/* resolution defined in UK MHEG Profile */
		d->xres = MHEG_XRES;
		d->yres = MHEG_YRES;
	}

	/* create the window */
	x = (WidthOfScreen(DefaultScreenOfDisplay(d->dpy)) - d->xres) / 2;
	y = (HeightOfScreen(DefaultScreenOfDisplay(d->dpy)) - d->yres) / 2;

	/* remember the colour depth and Visual used by the Window */
	d->depth = DefaultDepth(d->dpy, DefaultScreen(d->dpy));
	if(!XMatchVisualInfo(d->dpy, DefaultScreen(d->dpy), d->depth, TrueColor, &visinfo))
		fatal("Unable to find a TrueColour Visual");
	d->vis = visinfo.visual;

	/*
	 * if the default Visual is not TrueColor we need to create a Colormap for our Visual
	 * this is probably only gonna happen for 8 bit displays
	 * you *really* want a 16 or 24 bit colour display
	 */
	if(DefaultVisual(d->dpy, DefaultScreen(d->dpy))->class != TrueColor)
	{
		d->cmap = XCreateColormap(d->dpy, DefaultRootWindow(d->dpy), d->vis, AllocNone);
		XInstallColormap(d->dpy, d->cmap);
		mask = CWColormap;
		attr.colormap = d->cmap;
	}
	else
	{
		d->cmap = None;
		mask = 0;
	}

	/* don't need any special toolkits or widgets, just a canvas to draw on */
	d->win = XCreateWindow(d->dpy,
			       DefaultRootWindow(d->dpy),
			       x, y, d->xres, d->yres, 0,
			       d->depth, InputOutput, d->vis,
			       mask, &attr);

	/* in case the WM ignored where we want it placed */
	hint.flags = USSize | USPosition | PPosition | PSize;
	hint.x = x;
	hint.y = y;
	hint.width = d->xres;
	hint.height = d->yres;
	XSetWMNormalHints(d->dpy, d->win, &hint);

	if(fullscreen)
	{
		GC gc;
		Pixmap no_pixmap;
		XColor no_colour;
		Cursor no_cursor;
		/* this is how GDK makes Windows fullscreen */
		XEvent xev;
		Atom motif_wm_hints;
		MotifWmHints mwmhints;
		xev.xclient.type = ClientMessage;
		xev.xclient.serial = 0;
		xev.xclient.send_event = True;
		xev.xclient.window = d->win;
		xev.xclient.message_type = XInternAtom(d->dpy, "_NET_WM_STATE", False);
		xev.xclient.format = 32;
		xev.xclient.data.l[0] = XInternAtom(d->dpy, "_NET_WM_STATE_ADD", False);
		xev.xclient.data.l[1] = XInternAtom(d->dpy, "_NET_WM_STATE_FULLSCREEN", False);
		xev.xclient.data.l[2] = 0;
		xev.xclient.data.l[3] = 0;
		xev.xclient.data.l[4] = 0;
		XSendEvent(d->dpy, DefaultRootWindow(d->dpy), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
		/* get rid of the Window decorations */
		motif_wm_hints = XInternAtom(d->dpy, "_MOTIF_WM_HINTS", False);
		mwmhints.flags = MWM_HINTS_DECORATIONS;
		mwmhints.decorations = 0;
		XChangeProperty(d->dpy, d->win,
				motif_wm_hints, motif_wm_hints, 32, PropModeReplace,
				(unsigned char *) &mwmhints, sizeof(MotifWmHints) / sizeof(long));
		/*
		 * get rid of the cursor when the mouse is over our Window
		 * make the cursor a 1x1 Pixmap with no pixels displayed by the mask
		 */
		no_pixmap = XCreatePixmap(d->dpy, d->win, 1, 1, 1);
		gcmask = GCForeground;
		gcvals.foreground = 0;
		gc = XCreateGC(d->dpy, no_pixmap, gcmask, &gcvals);
		XDrawPoint(d->dpy, no_pixmap, gc, 0, 0);
		XFreeGC(d->dpy, gc);
		no_cursor = XCreatePixmapCursor(d->dpy, no_pixmap, no_pixmap, &no_colour, &no_colour, 0, 0);
		XFreePixmap(d->dpy, no_pixmap);
		XDefineCursor(d->dpy, d->win, no_cursor);
	}

	/* want to get Expose and KeyPress events */
	mask = CWEventMask;
	attr.event_mask = ExposureMask | KeyPressMask;
	XChangeWindowAttributes(d->dpy, d->win, mask, &attr);

	/* get a ClientMessage event when the window's close button is clicked */
	wm_delete_window = XInternAtom(d->dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(d->dpy, d->win, &wm_delete_window, 1);

	/* create an XRender Picture to do the drawing on */
	d->contents = XCreatePixmap(d->dpy, d->win, d->xres, d->yres, d->depth);
	pic_format = XRenderFindVisualFormat(d->dpy, d->vis);
	d->contents_pic = XRenderCreatePicture(d->dpy, d->contents, pic_format, 0, NULL);

	/* a 1x1 Picture to hold the text foreground colour */
	textfg = XCreatePixmap(d->dpy, d->win, 1, 1, d->depth);
	pa.repeat = True;
	d->textfg_pic = XRenderCreatePicture(d->dpy, textfg, pic_format, CPRepeat, &pa);

	/* a GC to draw on the Window */
	d->win_gc = XCreateGC(d->dpy, d->win, 0, &gcvals);

	/* get the window on the screen */
	XMapWindow(d->dpy, d->win);

	/* needs to be realised before we can set the title */
	XStoreName(d->dpy, d->win, WINDOW_TITLE);

	/* rather than having to implement our own timers we use Xt, so initialise it */
	XtToolkitInitialize();
	d->app = XtCreateApplicationContext();
	XtDisplayInitialize(d->app, d->dpy, APP_NAME, APP_CLASS, NULL, 0, &argc, argv);

	/* init ffmpeg */
	av_register_all();

	return;
}

void
MHEGDisplay_fini(MHEGDisplay *d)
{
	/* calls XCloseDisplay for us which free's all our Windows, Pixmaps, etc */
	XtDestroyApplicationContext(d->app);

	return;
}

/*
 * process the next GUI event
 * if block is false and no events are pending, return immediately
 * if block is true and no events are pending, wait for the next event
 * returns true if the GUI wants us to quit
 */

bool
MHEGDisplay_processEvents(MHEGDisplay *d, bool block)
{
	bool quit = false;
	XEvent event;
	XAnyEvent *any;
	XKeyEvent *key;
	KeySym sym;
	MHEGKeyMapEntry *map;
	XExposeEvent *exp;
	XClientMessageEvent *cm;
	static Atom wm_protocols = 0;
	static Atom wm_delete_window = 0;

	/* dont block if only a Timer is pending */
	if(!block
	&& (XtAppPending(d->app) & ~XtIMTimer) == 0)
		return false;

	/* this will block if no events are pending */
	XtAppNextEvent(d->app, &event);

	/* is it our window */
	any = &event.xany;
	if(any->display != d->dpy || any->window != d->win)
	{
		/* pass it on to Xt */
		XtDispatchEvent(&event);
		return false;
	}

	switch(event.type)
	{
	case KeyPress:
		key = &event.xkey;
		sym = XKeycodeToKeysym(d->dpy, key->keycode, 0);
		/* find the KeySym in the keyboard map */
		map = d->keymap;
		while(map->mheg_key != 0 && map->x_key != sym)
			map ++;
		if(map->mheg_key != 0)
		{
			verbose("KeyPress: %s (%u)", XKeysymToString(sym), map->mheg_key);
			MHEGEngine_keyPressed(map->mheg_key);
		}
		break;

	case Expose:
		exp = &event.xexpose;
		XCopyArea(d->dpy, d->contents, d->win, d->win_gc, exp->x, exp->y, exp->width, exp->height, exp->x, exp->y);
		break;

	case NoExpose:
		/* ignore it */
		break;

	case ClientMessage:
		cm = &event.xclient;
		/* cache these Atoms */
		if(wm_protocols == 0)
		{
			wm_protocols = XInternAtom(d->dpy, "WM_PROTOCOLS", False);
			wm_delete_window = XInternAtom(d->dpy, "WM_DELETE_WINDOW", False);
		}
		if(cm->message_type == wm_protocols
		&& cm->format == 32
		&& cm->data.l[0] == wm_delete_window)
			quit = true;
		else
			verbose("Ignoring ClientMessage type %s", XGetAtomName(d->dpy, cm->message_type));
		break;

	default:
		/* pass it on to Xt */
		XtDispatchEvent(&event);
		break;
	}

	return quit;
}

/*
 * gets the given area of the Window refreshed
 * coords should be in the range 0-MHEG_XRES, 0-MHEG_YRES
 */

void
MHEGDisplay_refresh(MHEGDisplay *d, XYPosition *pos, OriginalBoxSize *box)
{
	int x, y;
	unsigned int w, h;

	/* scale if fullscreen */
	x = (pos->x_position * d->xres) / MHEG_XRES;
	y = (pos->y_position * d->yres) / MHEG_YRES;
	w = (box->x_length * d->xres) / MHEG_XRES;
	h = (box->y_length * d->yres) / MHEG_YRES;

	XCopyArea(d->dpy, d->contents, d->win, d->win_gc, x, y, w, h, x, y);

	return;
}

/*
 * coords should be in the range 0-MHEG_XRES, 0-MHEG_YRES
 * width is the line width in pixels
 * style should be LineStyle_solid/dashed/dotted
 */

void
MHEGDisplay_drawHoriLine(MHEGDisplay *d, XYPosition *pos, unsigned int len, int width, int style, MHEGColour *col)
{
	XRenderColor rcol;
	int x, y;
	unsigned int w, h;

	/* if it is transparent or line width is <=0, just bail out */
	if(col->t == MHEGCOLOUR_TRANSPARENT || width <= 0)
		return;

	/* convert to internal colour format */
	display_colour(&rcol, col);

	/* scale if fullscreen */
	x = (pos->x_position * d->xres) / MHEG_XRES;
	y = (pos->y_position * d->yres) / MHEG_YRES;
	w = (len * d->xres) / MHEG_XRES;
	/* aspect ratio */
	h = (width * d->yres) / MHEG_YRES;

/* TODO */
if(style != LineStyle_solid)
printf("TODO: LineStyle %d\n", style);

	/* draw a rectangle */
	XRenderFillRectangle(d->dpy, PictOpOver, d->contents_pic, &rcol, x, y, w, h);

	return;
}

/*
 * coords should be in the range 0-MHEG_XRES, 0-MHEG_YRES
 * width is the line width in pixels
 * style should be LineStyle_solid/dashed/dotted
 */

void
MHEGDisplay_drawVertLine(MHEGDisplay *d, XYPosition *pos, unsigned int len, int width, int style, MHEGColour *col)
{
	XRenderColor rcol;
	int x, y;
	unsigned int w, h;

	/* if it is transparent or line width is <=0, just bail out */
	if(col->t == MHEGCOLOUR_TRANSPARENT || width <= 0)
		return;

	/* convert to internal colour format */
	display_colour(&rcol, col);

	/* scale if fullscreen */
	x = (pos->x_position * d->xres) / MHEG_XRES;
	y = (pos->y_position * d->yres) / MHEG_YRES;
	h = (len * d->yres) / MHEG_YRES;
	/* aspect ratio */
	w = (width * d->xres) / MHEG_XRES;

/* TODO */
if(style != LineStyle_solid)
printf("TODO: LineStyle %d\n", style);

	/* draw a rectangle */
	XRenderFillRectangle(d->dpy, PictOpOver, d->contents_pic, &rcol, x, y, w, h);

	return;
}

/*
 * coords should be in the range 0-MHEG_XRES, 0-MHEG_YRES
 */

void
MHEGDisplay_fillRectangle(MHEGDisplay *d, XYPosition *pos, OriginalBoxSize *box, MHEGColour *col)
{
	XRenderColor rcol;
	int x, y;
	unsigned int w, h;

	/* if it is transparent, just bail out */
	if(col->t == MHEGCOLOUR_TRANSPARENT)
		return;

	/* convert to internal colour format */
	display_colour(&rcol, col);

	/* scale if fullscreen */
	x = (pos->x_position * d->xres) / MHEG_XRES;
	y = (pos->y_position * d->yres) / MHEG_YRES;
	w = (box->x_length * d->xres) / MHEG_XRES;
	h = (box->y_length * d->yres) / MHEG_YRES;

	XRenderFillRectangle(d->dpy, PictOpOver, d->contents_pic, &rcol, x, y, w, h);

	return;
}

/*
 * coords should be in the range 0-MHEG_XRES, 0-MHEG_YRES
 */

void
MHEGDisplay_drawBitmap(MHEGDisplay *d, XYPosition *src, OriginalBoxSize *box, MHEGBitmap *bitmap, XYPosition *dst)
{
	int src_x, src_y;
	int dst_x, dst_y;
	unsigned int w, h;

	/* in case we don't have any content yet, UK MHEG Profile says make it transparent */
	if(bitmap == NULL)
		return;

	/*
	 * scale up if fullscreen
	 * the bitmap itself is scaled when it is created in MHEGDisplay_newBitmap()
	 */
	src_x = (src->x_position * d->xres) / MHEG_XRES;
	src_y = (src->y_position * d->yres) / MHEG_YRES;
	w = (box->x_length * d->xres) / MHEG_XRES;
	h = (box->y_length * d->yres) / MHEG_YRES;
	dst_x = (dst->x_position * d->xres) / MHEG_XRES;
	dst_y = (dst->y_position * d->yres) / MHEG_YRES;

	XRenderComposite(d->dpy, PictOpOver, bitmap->image_pic, None, d->contents_pic,
			 src_x, src_y, src_x, src_y, dst_x, dst_y, w, h);


	return;
}

/*
 * coords should be in the range 0-MHEG_XRES, 0-MHEG_YRES
 * text can include multibyte (UTF8) chars as described in the UK MHEG Profile
 * text can also include tab characters (0x09)
 * if tabs is false, tab characters are just treated as spaces
 * text should *not* include ESC sequences to change colour or \r for new lines
 */

void
MHEGDisplay_drawTextElement(MHEGDisplay *d, XYPosition *pos, MHEGFont *font, MHEGTextElement *text, bool tabs)
{
	XRenderColor rcol;
	int orig_x;
	int x, y;
	int scrn_x;
	FT_Face face;
	FT_GlyphSlot slot;
	FT_UInt glyph;
	FT_UInt previous;
	FT_Vector kern;
	FT_Error err;
	unsigned char *data;
	unsigned int size;
	int utf8;
	int len;
	int ntabs;

	/* is there any text */
	if(text->size == 0)
		return;

	/* convert to internal colour format */
	display_colour(&rcol, &text->col);

	/* scale the x origin if fullscreen */
	orig_x = (pos->x_position * d->xres) / MHEG_XRES;
	/* y coord does not change */
	y = ((pos->y_position + text->y) * d->yres) / MHEG_YRES;

	/* set the text foreground colour */
	XRenderFillRectangle(d->dpy, PictOpSrc, d->textfg_pic, &rcol, 0, 0, 1, 1);

	/*
	 * can't just use XftTextRenderUtf8() because:
	 * - it doesn't do kerning
	 * - text may include tabs
	 */
	/* we do all layout calculations with the unscaled font metrics */
	face = XftLockFace(font->font);
	slot = face->glyph;

	/* no previous glyph yet */
	previous = 0;

	/* x in font units */
	x = text->x * face->units_per_EM;

	data = text->data;
	size = text->size;
	while(size > 0)
	{
		/* get the next UTF8 char */
		utf8 = next_utf8(data, size, &len);
		data += len;
		size -= len;
		/* if it's a tab, just advance to the next tab stop */
		if(utf8 == 0x09 && tabs)
		{
			/* min amount a tab should advance the text pos */
			x += font->xOffsetLeft * face->units_per_EM;
			/* move to the next tab stop */
			ntabs = x / (MHEG_TAB_WIDTH * face->units_per_EM);
			x = ((ntabs + 1) * MHEG_TAB_WIDTH) * face->units_per_EM;
			continue;
		}
		/* we are treating tabs as spaces */
		if(utf8 == 0x09)
			utf8 = 0x20;
		/* get the glyph index for the UTF8 char */
		glyph = FT_Get_Char_Index(face, utf8);
		/* do any kerning if necessary */
		if(previous != 0 && FT_HAS_KERNING(face))
		{
			FT_Get_Kerning(face, previous, glyph, FT_KERNING_UNSCALED, &kern);
			x += (kern.x * font->size * 45) / 56;
		}
		/* remember the glyph for kerning next time */
		previous = glyph;
		/* render it */
		XftUnlockFace(font->font);
		/* round up/down the X coord */
		scrn_x = (x * d->xres) / MHEG_XRES;
		scrn_x = (scrn_x + (face->units_per_EM / 2)) / face->units_per_EM;
		XftGlyphRender(d->dpy, PictOpOver, d->textfg_pic, font->font, d->contents_pic,
			       0, 0, orig_x + scrn_x, y,
			       &glyph, 1);
		face = XftLockFace(font->font);
		slot = face->glyph;
		/* advance x */
		err = FT_Load_Glyph(face, glyph, FT_LOAD_NO_SCALE);
		if(err)
			continue;
		x += (slot->advance.x * font->size * 45) / 56;
		/* add on (letter spacing / 256) * units_per_EM */
		x += (face->units_per_EM * font->letter_spc * 45) / (256 * 56);
	}

	XftUnlockFace(font->font);

	return;
}

/*
 * convert the given PNG data to an internal format
 * returns NULL on error
 */

MHEGBitmap *
MHEGDisplay_newPNGBitmap(MHEGDisplay *d, OctetString *png)
{
	MHEGBitmap *b;
	png_uint_32 width, height;
	unsigned char *rgba;

	/* nothing to do */
	if(png == NULL || png->size == 0)
		return NULL;

	/* convert the PNG into a standard format we can use as an XImage */
	if((rgba = readpng_get_image(png->data, png->size, &width, &height)) == NULL)
	{
		error("Unable to decode PNG file");
		return NULL;
	}

	/*
	 * we now have an array of 32-bit RGBA pixels in network byte order
	 * ie if pix is a char *: pix[0] = R, pix[1] = G, pix[2] = B, pix[3] = A
	 */
	b = MHEGBitmap_fromRGBA(d, rgba, width, height);

	/* clean up */
	readpng_free_image(rgba);

	return b;
}

/*
 * convert the given MPEG I-frame data to an internal format
 * returns NULL on error
 */

MHEGBitmap *
MHEGDisplay_newMPEGBitmap(MHEGDisplay *d, OctetString *mpeg)
{
	MHEGBitmap *b;
	mpeg2dec_t *decoder;
	const mpeg2_info_t *info;
	bool done;
	unsigned int width = 0;		/* keep the compiler happy */
	unsigned int height = 0;	/* keep the compiler happy */
	unsigned char *rgba = NULL;	/* keep the compiler happy */
	unsigned int i;

	/* nothing to do */
	if(mpeg == NULL || mpeg->size == 0)
		return NULL;

	/* use libmpeg2 to convert the data into a standard format we can use as an XImage */
	if((decoder = mpeg2_init()) == NULL)
	{
		error("Unable to initialise MPEG decoder");
		return NULL;
	}
	info = mpeg2_info(decoder);

	/* feed all the data into the buffer */
	mpeg2_buffer(decoder, mpeg->data, mpeg->data + mpeg->size);

	done = false;
	do
	{
		mpeg2_state_t state = mpeg2_parse(decoder);
		switch(state)
		{
		case STATE_BUFFER:
			/*
			 * we've already filled the buffer with all the data
			 * if the decoder needs more, the data is probably invalid
			 */
			error("Unable to decode MPEG image");
			mpeg2_close(decoder);
			return NULL;

		case STATE_SEQUENCE:
			mpeg2_convert(decoder, mpeg2convert_rgb32, NULL);
			break;

		case STATE_SLICE:
		case STATE_END:
		case STATE_INVALID_END:
			/* do we have a picture */
			if(info->display_fbuf)
			{
				width = info->sequence->width;
				height = info->sequence->height;
				rgba = info->display_fbuf->buf[0];
				/* set the alpha channel values to opaque */
				for(i=0; i<width*height; i++)
					rgba[(i * 4) + 3] = 0xff;
				done = true;
			}
			break;

		default:
			break;
		}
	}
	while(!done);

	/*
	 * we now have an array of 32-bit RGBA pixels in network byte order
	 * ie if pix is a char *: pix[0] = R, pix[1] = G, pix[2] = B, pix[3] = A
	 */
	b = MHEGBitmap_fromRGBA(d, rgba, width, height);

	/* clean up */
	mpeg2_close(decoder);

	return b;
}

void
MHEGDisplay_freeBitmap(MHEGDisplay *d, MHEGBitmap *b)
{
	if(b != NULL)
	{
		XRenderFreePicture(d->dpy, b->image_pic);
		XFreePixmap(d->dpy, b->image);
		safe_free(b);
	}

	return;
}

/*
 * construct a MHEGBitmap from an array of 32-bit RGBA pixels in network byte order
 * ie if pix is a char *: pix[0] = R, pix[1] = G, pix[2] = B, pix[3] = A
 */

MHEGBitmap *
MHEGBitmap_fromRGBA(MHEGDisplay *d, unsigned char *rgba, unsigned int width, unsigned int height)
{
	MHEGBitmap *bitmap;
	unsigned char *xdata;
	uint32_t xpix;
	unsigned char r, g, b, a;
	unsigned int i, npixs;
	XImage *ximg;
	XRenderPictFormat *pic_format;
	GC gc;

	bitmap = safe_malloc(sizeof(MHEGBitmap));
	bzero(bitmap, sizeof(MHEGBitmap));

	/* find a matching XRender pixel format */
	pic_format = XRenderFindStandardFormat(d->dpy, PictStandardARGB32);

	/* copy the RGBA values into a block we can use as XImage data */
	npixs = width * height;
	/* 4 bytes per pixel */
	xdata = safe_malloc(npixs * 4);
	/*
	 * copy the pixels, converting them to our XRender RGBA order as we go
	 * even if the XRender pixel layout is the same as the PNG one we still process each pixel
	 * because we want to make sure transparent pixels have 0 for their RGB components
	 * otherwise, if we scale up the bitmap in fullscreen mode and apply our bilinear filter,
	 * we may end up with a border around the image
	 * this happens, for example, with the BBC's "Press Red" image
	 * it has a transparent box around it, but the RGB values are not 0 in the transparent area
	 * when we scale it up we get a pink border around it
	 */
	for(i=0; i<npixs; i++)
	{
		r = rgba[(i * 4) + 0];
		g = rgba[(i * 4) + 1];
		b = rgba[(i * 4) + 2];
		a = rgba[(i * 4) + 3];
		/* is it transparent */
		if(a == 0)
		{
			xpix = 0;
		}
		else
		{
			xpix = a << pic_format->direct.alpha;
			xpix |= r << pic_format->direct.red;
			xpix |= g << pic_format->direct.green;
			xpix |= b << pic_format->direct.blue;
		}
		*((uint32_t *) &xdata[i * 4]) = xpix;
	}

	/* get X to draw the XImage onto a Pixmap */
	if((ximg = XCreateImage(d->dpy, d->vis, 32, ZPixmap, 0, xdata, width, height, 32, 0)) == NULL)
		fatal("XCreateImage failed");
	bitmap->image = XCreatePixmap(d->dpy, d->win, width, height, 32);
	gc = XCreateGC(d->dpy, bitmap->image, 0, NULL);
	XPutImage(d->dpy, bitmap->image, gc, ximg, 0, 0, 0, 0, width, height);
	XFreeGC(d->dpy, gc);

	/* associate a Picture with it */
	bitmap->image_pic = XRenderCreatePicture(d->dpy, bitmap->image, pic_format, 0, NULL);

	/* if we are using fullscreen mode, scale the image */
	if(d->fullscreen)
	{
		/* set up the matrix to scale it */
		XTransform xform;
		/* X */
		xform.matrix[0][0] = (MHEG_XRES << 16) / d->xres;
		xform.matrix[0][1] = 0;
		xform.matrix[0][2] = 0;
		/* Y */
		xform.matrix[1][0] = 0;
		xform.matrix[1][1] = (MHEG_YRES << 16) / d->yres;
		xform.matrix[1][2] = 0;
		/* Z */
		xform.matrix[2][0] = 0;
		xform.matrix[2][1] = 0;
		xform.matrix[2][2] = 1 << 16;
		/* scale it */
		XRenderSetPictureTransform(d->dpy, bitmap->image_pic, &xform);
		/* set a filter to smooth the edges */
		XRenderSetPictureFilter(d->dpy, bitmap->image_pic, FilterBilinear, 0, 0);
	}

	/* we alloc'ed the XImage data, make sure XDestroyImage doesn't try to free it */
	safe_free(xdata);
	ximg->data = NULL;
	XDestroyImage(ximg);

	return bitmap;
}

/*
 * returns true if the two boxes intersect
 * sets out_pos and out_box to the intersection
 */

bool
intersects(XYPosition *p1, OriginalBoxSize *b1, XYPosition *p2, OriginalBoxSize *b2, XYPosition *out_pos, OriginalBoxSize *out_box)
{
	int x1 = p1->x_position;
	int y1 = p1->y_position;
	int w1 = b1->x_length;
	int h1 = b1->y_length;
	int x2 = p2->x_position;
	int y2 = p2->y_position;
	int w2 = b2->x_length;
	int h2 = b2->y_length;
	bool hmatch;
	bool vmatch;

	/* intersection */
	out_pos->x_position = MAX(x1, x2);
	out_pos->y_position = MAX(y1, y2);
	out_box->x_length = MIN(x1 + w1, x2 + w2) - out_pos->x_position;
	out_box->y_length = MIN(y1 + h1, y2 + h2) - out_pos->y_position;

	/* does it intersect */
	hmatch = (x1 < (x2 + w2)) && ((x1 + w1) > x2);
	vmatch = (y1 < (y2 + h2)) && ((y1 + h1) > y2);

	return hmatch && vmatch;
}

/*
 * load the given key map config file
 */

static MHEGKeyMapEntry *
load_keymap(char *filename)
{
	FILE *conf;
	MHEGKeyMapEntry *map;
	char symname[64];
	size_t len;

	if((conf = fopen(filename, "r")) == NULL)
		fatal("Unable to open keymap config '%s': %s", filename, strerror(errno));

	/* overwrite default_keymap */
	map = default_keymap;
	while(map->mheg_key != 0)
	{
		if(fgets(symname, sizeof(symname), conf) == NULL)
			fatal("Keymap config file '%s' ended unexpectedly", filename);
		/* chop off any trailing \n */
		len = strlen(symname);
		if(symname[len-1] == '\n')
			symname[len-1] = '\0';
		if((map->x_key = XStringToKeysym(symname)) == NoSymbol)
			fatal("Key '%s' does not exist", symname);
		map ++;
	}

	fclose(conf);

	return default_keymap;
}

/*
 * convert MHEGColour to internal format
 */

static void
display_colour(XRenderColor *out, MHEGColour *in)
{
	/* expand to 16 bits per channel */
	out->red = (in->r << 8) | in->r;
	out->green = (in->g << 8) | in->g;
	out->blue = (in->b << 8) | in->b;

	/* XRender has 0 as transparent and 65535 as opaque */
	out->alpha = ((255 - in->t) << 8) | (255 - in->t);

	return;
}

