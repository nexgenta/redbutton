/*
 * MHEGCanvas.c
 */

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

