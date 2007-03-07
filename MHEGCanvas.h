/*
 * MHEGCanvas.h
 */

#ifndef __MHEGCANVAS_H__
#define __MHEGCANVAS_H__

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

typedef struct
{
	Pixmap contents;	/* current image */
	Picture contents_pic;	/* XRender wrapper */
} MHEGCanvas;

MHEGCanvas *new_MHEGCanvas(unsigned int, unsigned int);
void free_MHEGCanvas(MHEGCanvas *);

#endif	/* __MHEGCANVAS_H__ */

