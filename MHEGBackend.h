/*
 * MHEGBackend.h
 */

#ifndef __MHEGBACKEND_H__
#define __MHEGBACKEND_H__

#include <stdio.h>
#include <stdbool.h>

typedef struct MHEGBackend
{
	char *base_dir;								/* Service Gateway root directory */
	bool (*checkContentRef)(struct MHEGBackend *, ContentReference *);	/* check a file exists */
	bool (*loadFile)(struct MHEGBackend *, OctetString *, OctetString *);	/* load a file */
	FILE *(*openFile)(struct MHEGBackend *, OctetString *, char *);		/* open a file */
	int (*closeFile)(struct MHEGBackend *, FILE *);				/* close a file */
} MHEGBackend;

MHEGBackend *new_MHEGBackend(bool, char *);
void free_MHEGBackend(MHEGBackend *);

#endif	/* __MHEGBACKEND_H__ */
