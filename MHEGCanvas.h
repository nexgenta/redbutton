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
	GC gc;			/* contains the clip mask for the border */
} MHEGCanvas;

MHEGCanvas *new_MHEGCanvas(unsigned int, unsigned int);
void free_MHEGCanvas(MHEGCanvas *);

void MHEGCanvas_setBorder(MHEGCanvas *, int, int, MHEGColour *);

void MHEGCanvas_clear(MHEGCanvas *, MHEGColour *);

#endif	/* __MHEGCANVAS_H__ */

