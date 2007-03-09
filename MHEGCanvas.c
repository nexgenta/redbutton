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
}

/*
 * set a border, no drawing will be done in the border (apart from the border itself)
 * width is in pixels
 * style should be one of:
 * original_line_style_solid
 * original_line_style_dashed
 * original_line_style_dotted
 */

void
MHEGCanvas_setBorder(MHEGCanvas *c, int width, int style, MHEGColour *colour)
{
	if(width <= 0)
		return;

/* TODO */
printf("TODO: MHEGCanvas_setBorder: width=%d style=%d RGBT=%02x%02x%02x%02x\n", width, style, colour->r, colour->g, colour->b, colour->t);

	return;
}

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

