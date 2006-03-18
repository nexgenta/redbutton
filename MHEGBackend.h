/*
 * MHEGBackend.h
 */

#ifndef __MHEGBACKEND_H__
#define __MHEGBACKEND_H__

#include <stdio.h>
#include <stdbool.h>
#include <netinet/in.h>

/* default TCP port to contact backend on */
#define DEFAULT_REMOTE_PORT	10101

typedef struct MHEGBackend
{
	char *base_dir;			/* local Service Gateway root directory */
	struct sockaddr_in addr;	/* remote backend IP and port */
	/* function pointers */
	struct MHEGBackendFns
	{
		bool (*checkContentRef)(struct MHEGBackend *, ContentReference *);	/* check a carousel file exists */
		bool (*loadFile)(struct MHEGBackend *, OctetString *, OctetString *);	/* load a carousel file */
		FILE *(*openFile)(struct MHEGBackend *, OctetString *);			/* open a carousel file */
		FILE *(*openStream)(struct MHEGBackend *, bool, int *, bool, int *);	/* open an MPEG Transport Stream */
	} *fns;
} MHEGBackend;

void MHEGBackend_init(MHEGBackend *, bool, char *);
void MHEGBackend_fini(MHEGBackend *);

#endif	/* __MHEGBACKEND_H__ */
