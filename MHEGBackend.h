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
	FILE *be_sock;			/* connection to remote backend */
	/* function pointers */
	struct MHEGBackendFns
	{
		/* check a carousel file exists */
		bool (*checkContentRef)(struct MHEGBackend *, ContentReference *);
		/* load a carousel file */
		bool (*loadFile)(struct MHEGBackend *, OctetString *, OctetString *);
		/* open a carousel file */
		FILE *(*openFile)(struct MHEGBackend *, OctetString *);
		/* open an MPEG Transport Stream */
		FILE *(*openStream)(struct MHEGBackend *, bool, int *, int *, bool, int *, int *);
		/* tune to the given service */
		void (*retune)(struct MHEGBackend *, OctetString *);
	} *fns;
} MHEGBackend;

void MHEGBackend_init(MHEGBackend *, bool, char *);
void MHEGBackend_fini(MHEGBackend *);

#endif	/* __MHEGBACKEND_H__ */
