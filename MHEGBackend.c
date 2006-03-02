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
FILE *local_openFile(MHEGBackend *, OctetString *, char *);
int local_closeFile(MHEGBackend *, FILE *);

static struct MHEGBackendFns local_backend_fns =
{
	local_checkContentRef,	/* checkContentRef */
	local_loadFile,		/* loadFile */
	local_openFile,		/* openFile */
	local_closeFile		/* closeFile */
};

/* remote backend funcs */
bool remote_checkContentRef(MHEGBackend *, ContentReference *);
bool remote_loadFile(MHEGBackend *, OctetString *, OctetString *);
FILE *remote_openFile(MHEGBackend *, OctetString *, char *);
int remote_closeFile(MHEGBackend *, FILE *);

static struct MHEGBackendFns remote_backend_fns =
{
	remote_checkContentRef,	/* checkContentRef */
	remote_loadFile,	/* loadFile */
	remote_openFile,	/* openFile */
	remote_closeFile	/* closeFile */
};

/* internal functions */
static int parse_addr(char *, struct in_addr *, in_port_t *);
static int get_host_addr(char *, struct in_addr *);

static int remote_command(MHEGBackend *, char *);
static unsigned int remote_response(int);

/* public interface */
void
MHEGBackend_init(MHEGBackend *b, bool remote, char *srg_loc)
{
	bzero(b, sizeof(MHEGBackend));

	if(remote)
	{
		/* backend is on a different host, srg_loc is the remote host[:port] */
		b->fns = &remote_backend_fns;
		b->addr.sin_family = AF_INET;
		b->addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		b->addr.sin_port = htons(DEFAULT_REMOTE_PORT);
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
 * returns a socket fd to read the response from
 * returns <0 if it can't contact the backend
 */

static int
remote_command(MHEGBackend *t, char *cmd)
{
	int sock;

	/* connect to the backend */
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		error("Unable to create backend socket: %s", strerror(errno));
		return -1;
	}

	if(connect(sock, (struct sockaddr *) &t->addr, sizeof(struct sockaddr_in)) < 0)
	{
		error("Unable to connect to backend: %s", strerror(errno));
		return -1;
	}

	/* send the command */
	write_string(sock, cmd);

	return sock;
}

/*
 * read the backend response from the given socket fd
 * returns the OK/error code
 */

#define BACKEND_RESPONSE_OK	200

static unsigned int
remote_response(int sock)
{
	char buf[1024];
	char byte;
	unsigned int total;
	ssize_t nread;
	unsigned int rc;

	/* read upto \n */
	total = 0;
	do
	{
		if((nread = read(sock, &byte, 1)) == 1)
			buf[total++] = byte;
	}
	while(nread == 1 && byte != '\n' && total < (sizeof(buf) - 1));

	/* \0 terminate it */
	buf[total] = '\0';

	rc = atoi(buf);

	return rc;
}

/*
 * local routines
 */

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
	int sock;
	bool exists;

	snprintf(cmd, sizeof(cmd), "check %s\n", MHEGEngine_absoluteFilename(name));

	if((sock = remote_command(t, cmd)) < 0)
		return false;

	exists = (remote_response(sock) == BACKEND_RESPONSE_OK);

	close(sock);

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
	int sock;
	char buf[8 * 1024];
	ssize_t nread;
	bool success = false;

	snprintf(cmd, sizeof(cmd), "file %s\n", MHEGEngine_absoluteFilename(name));

	if((sock = remote_command(t, cmd)) < 0)
		return false;

	/* does it exist */
	if(remote_response(sock) == BACKEND_RESPONSE_OK)
	{
		verbose("Loading '%.*s'", name->size, name->data);
		/* read from the socket until EOF */
		do
		{
			if((nread = read(sock, buf, sizeof(buf))) > 0)
			{
				out->data = safe_realloc(out->data, out->size + nread);
				memcpy(out->data + out->size, buf, nread);
				out->size += nread;
			}
		}
		while(nread > 0);
		success = true;
	}
	else
	{
		error("Unable to load '%.*s'", name->size, name->data);
	}

	close(sock);

	return success;
}

FILE *
remote_openFile(MHEGBackend *t, OctetString *name, char *mode)
{
	char cmd[PATH_MAX];
	int sock;
	char buf[8 * 1024];
	ssize_t nread;
	size_t nwritten;
	FILE *file = NULL;

	snprintf(cmd, sizeof(cmd), "file %s\n", MHEGEngine_absoluteFilename(name));

	if((sock = remote_command(t, cmd)) < 0)
		return NULL;

	/* does it exist */
	if(remote_response(sock) == BACKEND_RESPONSE_OK)
	{
		if((file = tmpfile()) != NULL)
		{
			/* read from the socket until EOF */
			do
			{
				if((nread = read(sock, buf, sizeof(buf))) > 0)
					nwritten = fwrite(buf, 1, nread, file);
				else
					nwritten = 0;
			}
			while(nread > 0 && nread == nwritten);
			/* could we write the file ok */
			if(nread != nwritten)
			{
				error("Unable to write to local file");
				fclose(file);
				file = NULL;
			}
		}
	}

	close(sock);

	/* rewind the file */
	if(file != NULL)
		rewind(file);

	return file;
}

/*
 * close a FILE opened with MHEGEngine_openFile()
 */

int
remote_closeFile(MHEGBackend *t, FILE *file)
{
	/* tmpfile() will delete the file for us */
	return fclose(file);
}

