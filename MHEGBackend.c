/*
 * MHEGBackend.c
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "MHEGEngine.h"
#include "utils.h"

/* local backend funcs */
bool local_checkContentRef(MHEGBackend *, ContentReference *);
bool local_loadFile(MHEGBackend *, OctetString *, OctetString *);
FILE *local_openFile(MHEGBackend *, OctetString *);
FILE *local_openStream(MHEGBackend *, bool, int *, bool, int *);

static struct MHEGBackendFns local_backend_fns =
{
	local_checkContentRef,	/* checkContentRef */
	local_loadFile,		/* loadFile */
	local_openFile,		/* openFile */
	local_openStream,	/* openStream */
};

/* remote backend funcs */
bool remote_checkContentRef(MHEGBackend *, ContentReference *);
bool remote_loadFile(MHEGBackend *, OctetString *, OctetString *);
FILE *remote_openFile(MHEGBackend *, OctetString *);
FILE *remote_openStream(MHEGBackend *, bool, int *, bool, int *);

static struct MHEGBackendFns remote_backend_fns =
{
	remote_checkContentRef,	/* checkContentRef */
	remote_loadFile,	/* loadFile */
	remote_openFile,	/* openFile */
	remote_openStream,	/* openStream */
};

/* internal functions */
static int parse_addr(char *, struct in_addr *, in_port_t *);
static int get_host_addr(char *, struct in_addr *);

static FILE *remote_command(MHEGBackend *, char *);
static unsigned int remote_response(FILE *);

static char *external_filename(MHEGBackend *, OctetString *);

/* public interface */
void
MHEGBackend_init(MHEGBackend *b, bool remote, char *srg_loc)
{
	bzero(b, sizeof(MHEGBackend));

	/* default backend is on the loopback */
	b->addr.sin_family = AF_INET;
	b->addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	b->addr.sin_port = htons(DEFAULT_REMOTE_PORT);

	if(remote)
	{
		/* backend is on a different host, srg_loc is the remote host[:port] */
		b->fns = &remote_backend_fns;
		if(parse_addr(srg_loc, &b->addr.sin_addr, &b->addr.sin_port) < 0)
			fatal("Unable to resolve host %s", srg_loc);
		verbose("Remote backend at %s:%u", inet_ntoa(b->addr.sin_addr), ntohs(b->addr.sin_port));
	}
	else
	{
		/* backend and frontend on same host, srg_loc is the base directory */
		b->fns = &local_backend_fns;
		b->base_dir = srg_loc;
		verbose("Local backend; carousel file root '%s'", srg_loc);
	}

	return;
}

void
MHEGBackend_fini(MHEGBackend *b)
{
	return;
}

/*
 * extract the IP addr and port number from a string in one of these forms:
 * host:port
 * ip-addr:port
 * host
 * ip-addr
 * if the port is not defined in the string, the value passed to this routine is unchanged
 * ip and port are both returned in network byte order
 * returns -1 on error (can't resolve host name)
 */

static int
parse_addr(char *str, struct in_addr *ip, in_port_t *port)
{
	char *p;

	if((p = strchr(str, ':')) != NULL)
	{
		/* its either host:port or ip:port */
		*(p++) = '\0';
		if(get_host_addr(str, ip) < 0)
			return -1;
		*port = htons(atoi(p));
		/* reconstruct the string */
		*(--p) = ':';
	}
	else if(get_host_addr(str, ip) < 0)
	{
		return -1;
	}

	return 0;
}

/*
 * puts the IP address associated with the given host into output buffer
 * host can be a.b.c.d or a host name
 * returns 0 if successful, -1 on error
 */

static int
get_host_addr(char *host, struct in_addr *output)
{
	struct hostent *he;
	int error = 0;

	if(((he = gethostbyname(host)) != NULL) && (he->h_addrtype == AF_INET))
		memcpy(output, he->h_addr, sizeof(struct in_addr));
	else
		error = -1;

	return error;
}

/*
 * send the given command to the remote backend
 * returns a socket FILE to read the response from
 * returns NULL if it can't contact the backend
 */

static FILE *
remote_command(MHEGBackend *t, char *cmd)
{
	int sock;
	FILE *file;

	/* connect to the backend */
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		error("Unable to create backend socket: %s", strerror(errno));
		return NULL;
	}

	if(connect(sock, (struct sockaddr *) &t->addr, sizeof(struct sockaddr_in)) < 0)
	{
		error("Unable to connect to backend: %s", strerror(errno));
		close(sock);
		return NULL;
	}

	/* associate a FILE with the socket (so stdio can do buffering) */
	if((file = fdopen(sock, "r+")) != NULL)
	{
		/* send the command */
		fputs(cmd, file);
	}
	else
	{
		error("Unable to buffer backend connection: %s", strerror(errno));
		close(sock);
	}

	return file;
}

/*
 * read the backend response from the given socket FILE
 * returns the OK/error code
 */

#define BACKEND_RESPONSE_OK	200
#define BACKEND_RESPONSE_ERROR	500

static unsigned int
remote_response(FILE *file)
{
	char buf[1024];
	unsigned int rc;

	/* read upto \n */
	if(fgets(buf, sizeof(buf), file) == NULL)
		return BACKEND_RESPONSE_ERROR;

	rc = atoi(buf);

	return rc;
}

/*
 * returns a filename that can be loaded from the file system
 * ie ~// at the start of the absolute name is replaced with base_dir
 * returns a ptr to a static string that will be overwritten by the next call to this routine
 */

static char _external[PATH_MAX];

static char *
external_filename(MHEGBackend *t, OctetString *name)
{
	char *absolute;

	/* convert it to an absolute group id, ie with a "~//" at the start */
	absolute = MHEGEngine_absoluteFilename(name);

	/* construct the filename */
	snprintf(_external, sizeof(_external), "%s%s", t->base_dir, &absolute[2]);

	return _external;
}

/*
 * local routines
 */

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
 * out should be {0,NULL} before calling this
 */

bool
local_loadFile(MHEGBackend *t, OctetString *name, OctetString *out)
{
	char *fullname;
	FILE *file;

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

/*
 * return a read-only FILE handle for the given carousel file
 * returns NULL on error
 */

FILE *
local_openFile(MHEGBackend *t, OctetString *name)
{
	char *external = external_filename(t, name);

	return fopen(external, "r");
}

/*
 * return a read-only FILE handle for an MPEG Transport Stream
 * the TS will contain an audio stream (if have_audio is true) and a video stream (if have_video is true)
 * the *audio_tag and *video_tag numbers refer to Component/Association Tag values from the DVB PMT
 * if *audio_tag or *video_tag is -1, the default audio and/or video stream for the current Service ID is used
 * updates *audio_tag and/or *video_tag to the actual PIDs in the Transport Stream
 * returns NULL on error
 */

FILE *
local_openStream(MHEGBackend *t, bool have_audio, int *audio_tag, bool have_video, int *video_tag)
{
	/*
	 * we need to convert the audio/video_tag into PIDs
	 * we could either:
	 * 1. parse the PMT ourselves, and open the DVB device ourselves
	 * 2. have a backend command to convert Component Tags to PIDs, then open the DVB device ourselves
	 * 3. just stream the TS from the backend
	 * we choose 3, to avoid duplicating code and having to pass "-d <device>" options etc
	 */
	return remote_openStream(t, have_audio, audio_tag, have_video, video_tag);
}

/*
 * remote routines
 */

/*
 * returns true if the file exists on the carousel
 */

bool
remote_checkContentRef(MHEGBackend *t, ContentReference *name)
{
	char cmd[PATH_MAX];
	FILE *sock;
	bool exists;

	snprintf(cmd, sizeof(cmd), "check %s\n", MHEGEngine_absoluteFilename(name));

	if((sock = remote_command(t, cmd)) == NULL)
		return false;

	exists = (remote_response(sock) == BACKEND_RESPONSE_OK);

	fclose(sock);

	return exists;
}

/*
 * file contents are stored in out (out->data will need to be free'd)
 * returns false if it can't load the file (out will be {0,NULL})
 * out should be {0,NULL} before calling this
 */

bool
remote_loadFile(MHEGBackend *t, OctetString *name, OctetString *out)
{
	char cmd[PATH_MAX];
	FILE *sock;
	char buf[8 * 1024];
	size_t nread;
	bool success = false;

	snprintf(cmd, sizeof(cmd), "file %s\n", MHEGEngine_absoluteFilename(name));

	if((sock = remote_command(t, cmd)) == NULL)
		return false;

	/* does it exist */
	if(remote_response(sock) == BACKEND_RESPONSE_OK)
	{
		verbose("Loading '%.*s'", name->size, name->data);
		/* read from the socket until EOF */
		while(!feof(sock))
		{
/* TODO */
/* could read straight into out->data rather than doing memcpy */
			if((nread = fread(buf, 1, sizeof(buf), sock)) > 0)
			{
				out->data = safe_realloc(out->data, out->size + nread);
				memcpy(out->data + out->size, buf, nread);
				out->size += nread;
			}
		}
		success = true;
	}
	else
	{
		error("Unable to load '%.*s'", name->size, name->data);
	}

	fclose(sock);

	return success;
}

/*
 * return a read-only FILE handle for the given carousel file
 * returns NULL on error
 */

FILE *
remote_openFile(MHEGBackend *t, OctetString *name)
{
	char cmd[PATH_MAX];
	FILE *sock;
	char buf[8 * 1024];
	size_t nread;
	size_t nwritten;
	FILE *out = NULL;

	snprintf(cmd, sizeof(cmd), "file %s\n", MHEGEngine_absoluteFilename(name));

	if((sock = remote_command(t, cmd)) == NULL)
		return NULL;

	/* does it exist */
	if(remote_response(sock) == BACKEND_RESPONSE_OK)
	{
		/* tmpfile() will delete the file when we fclose() it */
		if((out = tmpfile()) != NULL)
		{
			/* read from the socket until EOF */
			do
			{
				if((nread = fread(buf, 1, sizeof(buf), sock)) > 0)
					nwritten = fwrite(buf, 1, nread, out);
				else
					nwritten = 0;
			}
			while(!feof(sock) && nread == nwritten);
			/* could we write the file ok */
			if(nread != nwritten)
			{
				error("Unable to write to local file");
				fclose(out);
				out = NULL;
			}
		}
	}

	fclose(sock);

	/* rewind the file */
	if(out != NULL)
		rewind(out);

	return out;
}

/*
 * return a read-only FILE handle for an MPEG Transport Stream
 * the TS will contain an audio stream (if have_audio is true) and a video stream (if have_video is true)
 * the *audio_tag and *video_tag numbers refer to Component/Association Tag values from the DVB PMT
 * if *audio_tag or *video_tag is -1, the default audio and/or video stream for the current Service ID is used
 * updates *audio_tag and/or *video_tag to the actual PIDs in the Transport Stream
 * returns NULL on error
 */

FILE *
remote_openStream(MHEGBackend *t, bool have_audio, int *audio_tag, bool have_video, int *video_tag)
{
	char cmd[PATH_MAX];
	FILE *sock;
	char pids[128];
	unsigned int audio_pid = 0;
	unsigned int video_pid = 0;
	bool err;

	/* no PIDs required */
	if(!have_audio && !have_video)
		return NULL;
	/* video and audio */
	else if(have_audio && have_video)
		snprintf(cmd, sizeof(cmd), "avstream %d %d\n", *audio_tag, *video_tag);
	/* audio only */
	else if(have_audio)
		snprintf(cmd, sizeof(cmd), "astream %d\n", *audio_tag);
	/* video only */
	else
		snprintf(cmd, sizeof(cmd), "vstream %d\n", *video_tag);

	if((sock = remote_command(t, cmd)) == NULL)
		return NULL;

	/* did it work */
	if(remote_response(sock) != BACKEND_RESPONSE_OK
	|| fgets(pids, sizeof(pids), sock) == NULL)
	{
		fclose(sock);
		sock = NULL;
	}

	/* update the PID variables */
	if(have_audio && have_video)
		err = (sscanf(pids, "AudioPID %u VideoPID %u", &audio_pid, &video_pid) != 2);
	else if(have_audio)
		err = (sscanf(pids, "AudioPID %u", &audio_pid) != 1);
	else
		err = (sscanf(pids, "VideoPID %u", &video_pid) != 1);

	if(!err)
	{
		if(have_audio)
			*audio_tag = audio_pid;
		if(have_video)
			*video_tag = video_pid;
	}
	else
	{
		fclose(sock);
		sock = NULL;
	}

	return sock;
}

