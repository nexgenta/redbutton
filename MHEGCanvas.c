/*
 * MHEGCanvas.c
 */

#include "ISO13522-MHEG-5.h"
#include "MHEGCanvas.h"
#include "utils.h"

MHEGCanvas *
new_MHEGCanvas(unsigned int width, unsigned int height)
{
	MHEGCanvas *c = safe_mallocz(sizeof(MHEGCanvas));

/* TODO */
printf("TODO: new_MHEGCanvas(%u, %u)\n", width, height);

	return c;
}

void
free_MHEGCanvas(MHEGCanvas *c)
{
	safe_free(c);

	return;
}

/*
 * set a border, no drawing will be done in the border (apart from the border itself)
 * width is in pixels
 * style should be one of:
 * LineStyle_solid
 * LineStyle_dashed
 * LineStyle_dotted
 * (note: UK MHEG Profile says we can treat ALL line styles as solid)
 */

void
MHEGCanvas_setBorder(MHEGCanvas *c, int width, int style, MHEGColour *colour)
{
	if(width <= 0)
		return;

	if(style != LineStyle_solid)
		error("MHEGCanvas_setBorder: LineStyle %d not supported (using a solid line)", style);

/* TODO */
printf("TODO: MHEGCanvas_setBorder: width=%d style=%d RGBT=%02x%02x%02x%02x\n", width, style, colour->r, colour->g, colour->b, colour->t);

	return;
}

/*
 * drawing routine notes:
 * if border width is W, then coord {W,W} is the first pixel that will not be hidden by the border
 * UK MHEG Profile says we can treat ALL line styles as solid
 * UK MHEG Profile says no alpha blending is done within the DynamicLineArt canvas
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

