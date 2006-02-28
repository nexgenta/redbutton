/*
 * MHEGBackend.c
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "MHEGEngine.h"
#include "utils.h"

bool local_checkContentRef(MHEGBackend *, ContentReference *);
bool local_loadFile(MHEGBackend *, OctetString *, OctetString *);
FILE *local_openFile(MHEGBackend *, OctetString *, char *);
int local_closeFile(MHEGBackend *, FILE *);

static MHEGBackend local_backend =
{
	NULL,			/* base_dir */
	local_checkContentRef,	/* checkContentRef */
	local_loadFile,		/* loadFile */
	local_openFile,		/* openFile */
	local_closeFile		/* closeFile */
};

MHEGBackend *
new_MHEGBackend(bool remote, char *srg_loc)
{
	MHEGBackend *b = NULL;

	if(remote)
	{
		/* backend is on a different host, srg_loc is the remote host */
/* TODO */
fatal("TODO: remote backends not supported: %s", srg_loc);
	}
	else
	{
		/* backend and frontend on same host, srg_loc is the base directory */
		b = &local_backend;
		b->base_dir = srg_loc;
	}

	return b;
}

void
free_MHEGBackend(MHEGBackend *b)
{
	return;
}

/*
 * local routines
 */

/*
 * returns a filename that can be loaded from the file system
 * ie ~// at the start of the absolute name is replaced with base_dir
 * returns a ptr to static string that will be overwritten by the next call to this routine
 */

static char _external[PATH_MAX];

static char *
external_filename(MHEGBackend *t, OctetString *name)
{
	char *absolute;

	/* convert it to an absolute group id, ie with a "~//" at the start */
	absolute = MHEGEngine_absoluteFilename(name);

	/* construct the filename */
	snprintf(_external, sizeof(_external), "%s%s", t->base_dir, &absolute[1]);

	return _external;
}

/*
 * returns true if the file exists on the carousel
 */

bool
local_checkContentRef(MHEGBackend *t, ContentReference *name)
{
	bool found = false;
	FILE *file;

	if((file = fopen(external_filename(t, name), "r")) != NULL)
	{
		fclose(file);
		found = true;
	}

	return found;
}

/*
 * file contents are stored in out (out->data will need to be free'd)
 * returns false if it can't load the file (out will be {0,NULL})
 */

bool
local_loadFile(MHEGBackend *t, OctetString *name, OctetString *out)
{
	char *fullname;
	FILE *file;

	out->size = 0;
	out->data = NULL;

	/* just in case */
	if(name->size == 0)
	{
		verbose("local_loadFile: no filename given");
		return false;
	}

	fullname = external_filename(t, name);

	/* open it */
	if((file = fopen(fullname, "r")) == NULL)
	{
		error("Unable to open '%.*s': %s", name->size, name->data, strerror(errno));
		return false;
	}

	verbose("Loading '%.*s'", name->size, name->data);

	fseek(file, 0, SEEK_END);
	out->size = ftell(file);
	out->data = safe_malloc(out->size);
	rewind(file);
	if(fread(out->data, 1, out->size, file) != out->size)
	{
		error("Unable to load '%.*s'", name->size, name->data);
		safe_free(out->data);
		out->size = 0;
		out->data = NULL;
	}

	fclose(file);

	return (out->data != NULL);
}

FILE *
local_openFile(MHEGBackend *t, OctetString *name, char *mode)
{
	char *external = external_filename(t, name);

	return fopen(external, mode);
}

/*
 * close a FILE opened with MHEGEngine_openFile()
 */

int
local_closeFile(MHEGBackend *t, FILE *file)
{
	return fclose(file);
}

