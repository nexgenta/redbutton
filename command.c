/*
 * command.c
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>

#include "command.h"
#include "assoc.h"
#include "fs.h"
#include "stream.h"
#include "utils.h"

/* max number of args that can be passed to a command (arbitrary) */
#define ARGV_MAX	10

/* the commands */
bool cmd_astream(struct listen_data *, FILE *, int, char **);
bool cmd_avstream(struct listen_data *, FILE *, int, char **);
bool cmd_check(struct listen_data *, FILE *, int, char **);
bool cmd_file(struct listen_data *, FILE *, int, char **);
bool cmd_help(struct listen_data *, FILE *, int, char **);
bool cmd_quit(struct listen_data *, FILE *, int, char **);
bool cmd_vstream(struct listen_data *, FILE *, int, char **);

static struct
{
	char *name;
	char *args;
	bool (*proc)(struct listen_data *, FILE *, int, char **);
	char *help;
} command[] =
{
	{ "astream", "<ComponentTag>",		cmd_astream,	"Stream the given audio component tag" },
	{ "avstream", "<AudioTag> <VideoTag>",	cmd_avstream,	"Stream the given audio and video component tags" },
	{ "check", "<ContentReference>",	cmd_check,	"Check if the given file exists on the carousel" },
	{ "exit", "",				cmd_quit,	"Close the connection" },
	{ "file", "<ContentReference>",		cmd_file,	"Retrieve the given file from the carousel" },
	{ "help", "",				cmd_help,	"List available commands" },
	{ "quit", "",				cmd_quit,	"Close the connection" },
	{ "vstream", "<ComponentTag>",		cmd_vstream,	"Stream the given video component tag" },
	{ NULL, NULL, NULL, NULL }
};

/* send an OK/error code etc response down client_sock */
#define SEND_RESPONSE(RC, MESSAGE)	fputs(#RC " " MESSAGE "\n", client)

/* internal routines */
char *external_filename(struct listen_data *, char *);
char *canonical_filename(char *);

/*
 * process the given command
 * return true if we should close the connection
 */

bool
process_command(struct listen_data *listen_data, FILE *client, char *cmd)
{
	int argc;
	char *argv[ARGV_MAX];
	char term;
	unsigned int cmd_len;
	int i;

	/* chop it into words, complicated by quoting */
	argc = 0;
	while(argc < ARGV_MAX && *cmd != '\0')
	{
		argv[argc++] = cmd;
		/* do we need to find the next space, or the next quote */
		if(*cmd == '\'' || *cmd == '"')
		{
			term = *cmd;
			/* remove/skip the opening quote */
			argv[argc-1] ++;
			cmd ++;
		}
		else
		{
			/* stop at the next space */
			term = ' ';
		}
		/* find the next terminating character */
		while(*cmd != term && *cmd != '\0')
			cmd ++;
		/* if this is not the end, skip to the start of the next word */
		if(*cmd != '\0')
		{
			/* terminate the last word */
			*cmd = '\0';
			/* move onto the next non-space character */
			cmd = skip_ws(cmd + 1);
		}
	}

	cmd_len = strlen(argv[0]);
	for(i=0; command[i].name != NULL; i++)
	{
		if(strncmp(argv[0], command[i].name, cmd_len) == 0)
			return (command[i].proc)(listen_data, client, argc, argv);
	}

	SEND_RESPONSE(500, "Unrecognised command");

	return false;
}

/*
 * the commands
 * listen_data is global data needed by listener commands
 * client is where any response data should go
 * argc is the number of arguments passed to it
 * argv[0] is the command name (or the abreviation the user used)
 * return true if we should close the connection
 */

#define CHECK_USAGE(ARGC, SYNTAX)		\
if(argc != ARGC)				\
{						\
	SEND_RESPONSE(500, "Syntax: " SYNTAX);	\
	return false;				\
}

/*
 * astream <tag>
 * send the given audio stream down the connection
 * the tag should be an association/component_tag number as found in the PMT
 * the tag is converted to a PID and that PID is sent as a MPEG Transport Stream down the connection
 * if tag is -1, the default audio stream for the current service_id is sent
 */

bool
cmd_astream(struct listen_data *listen_data, FILE *client, int argc, char *argv[])
{
	struct carousel *car = listen_data->carousel;
	int tag;
	uint16_t pid;
	int audio_fd;
	int ts_fd;
	char hdr[64];

	CHECK_USAGE(2, "astream <ComponentTag>");

	tag = strtol(argv[1], NULL, 0);

	/* map the tag to a PID, or use the default */
	if(tag == -1)
		pid = car->audio_pid;
	else
		pid = stream2pid(&car->assoc, tag);

	/* add the PID to the demux device */
	if((audio_fd = add_demux_filter(car->demux_device, pid, DMX_PES_AUDIO)) < 0)
	{
		SEND_RESPONSE(500, "Unable to open audio PID");
		return false;
	}

	/* we can now read a transport stream from the dvr device */
	if((ts_fd = open(car->dvr_device, O_RDONLY)) < 0)
	{
		SEND_RESPONSE(500, "Unable to open DVB device");
		close(audio_fd);
		return false;
	}

	/* send the OK code */
	SEND_RESPONSE(200, "OK");

	/* tell the client what PID the component tag resolved to */
	snprintf(hdr, sizeof(hdr), "AudioPID %u\n", pid);
	fputs(hdr, client);

	/* shovel the transport stream to client until the client closes or we get an error */
	stream_ts(ts_fd, client);

	/* clean up */
	close(ts_fd);
	close(audio_fd);

	/* close the connection */
	return true;
}

/*
 * vstream <tag>
 * send the given video stream down the connection
 * the tag should be an association/component_tag number as found in the PMT
 * the tag is converted to a PID and that PID is sent as a MPEG Transport Stream down the connection
 * if tag is -1, the default video stream for the current service_id is sent
 */

bool
cmd_vstream(struct listen_data *listen_data, FILE *client, int argc, char *argv[])
{
	struct carousel *car = listen_data->carousel;
	int tag;
	uint16_t pid;
	int video_fd;
	int ts_fd;
	char hdr[64];

	CHECK_USAGE(2, "vstream <ComponentTag>");

	tag = strtol(argv[1], NULL, 0);

	/* map the tag to a PID, or use the default */
	if(tag == -1)
		pid = car->video_pid;
	else
		pid = stream2pid(&car->assoc, tag);

	/* add the PID to the demux device */
	if((video_fd = add_demux_filter(car->demux_device, pid, DMX_PES_VIDEO)) < 0)
	{
		SEND_RESPONSE(500, "Unable to open video PID");
		return false;
	}

	/* we can now read a transport stream from the dvr device */
	if((ts_fd = open(car->dvr_device, O_RDONLY)) < 0)
	{
		SEND_RESPONSE(500, "Unable to open DVB device");
		close(video_fd);
		return false;
	}

	/* send the OK code */
	SEND_RESPONSE(200, "OK");

	/* tell the client what PID the component tag resolved to */
	snprintf(hdr, sizeof(hdr), "VideoPID %u\n", pid);
	fputs(hdr, client);

	/* shovel the transport stream down client_sock until the client closes it or we get an error */
	stream_ts(ts_fd, client);

	/* clean up */
	close(ts_fd);
	close(video_fd);

	/* close the connection */
	return true;
}

/*
 * avstream <audio_tag> <video_tag>
 * send the given audio and video streams down the connection
 * the tags should be association/component_tag numbers as found in the PMT
 * the tags are converted to PIDs and those PIDs are sent as a MPEG Transport Stream down the connection
 * if a tag is -1, the default audio or video stream for the current service_id is sent
 */

bool
cmd_avstream(struct listen_data *listen_data, FILE *client, int argc, char *argv[])
{
	struct carousel *car = listen_data->carousel;
	int audio_tag;
	int video_tag;
	uint16_t audio_pid;
	uint16_t video_pid;
	int audio_fd;
	int video_fd;
	int ts_fd;
	char hdr[64];

	CHECK_USAGE(3, "avstream <AudioTag> <VideoTag>");

	audio_tag = strtol(argv[1], NULL, 0);
	video_tag = strtol(argv[2], NULL, 0);

	/* map the tags to PIDs, or use the defaults */
	if(audio_tag == -1)
		audio_pid = car->audio_pid;
	else
		audio_pid = stream2pid(&car->assoc, audio_tag);

	if(video_tag == -1)
		video_pid = car->video_pid;
	else
		video_pid = stream2pid(&car->assoc, video_tag);

	/* add the PIDs to the demux device */
	if((audio_fd = add_demux_filter(car->demux_device, audio_pid, DMX_PES_AUDIO)) < 0)
	{
		SEND_RESPONSE(500, "Unable to open audio PID");
		return false;
	}
	if((video_fd = add_demux_filter(car->demux_device, video_pid, DMX_PES_VIDEO)) < 0)
	{
		SEND_RESPONSE(500, "Unable to open video PID");
		close(audio_fd);
		return false;
	}

	/* we can now read a transport stream from the dvr device */
	if((ts_fd = open(car->dvr_device, O_RDONLY)) < 0)
	{
		SEND_RESPONSE(500, "Unable to open DVB device");
		close(audio_fd);
		close(video_fd);
		return false;
	}

	/* send the OK code */
	SEND_RESPONSE(200, "OK");

	/* tell the client what PIDs the component tags resolved to */
	snprintf(hdr, sizeof(hdr), "AudioPID %u VideoPID %u\n", audio_pid, video_pid);
	fputs(hdr, client);

	/* shovel the transport stream down client_sock until the client closes it or we get an error */
	stream_ts(ts_fd, client);

	/* clean up */
	close(ts_fd);
	close(audio_fd);
	close(video_fd);

	/* close the connection */
	return true;
}

/*
 * check <ContentReference>
 * check if the given file is on the carousel
 * ContentReference should be absolute, ie start with "~//"
 */

bool
cmd_check(struct listen_data *listen_data, FILE *client, int argc, char *argv[])
{
	char *filename;
	FILE *file;

	CHECK_USAGE(2, "check <ContentReference>");

	if((filename = external_filename(listen_data, argv[1])) == NULL)
	{
		SEND_RESPONSE(500, "Invalid ContentReference");
		return false;
	}

	if((file = fopen(filename, "r")) != NULL)
	{
		fclose(file);
		SEND_RESPONSE(200, "OK");
	}
	else
	{
		SEND_RESPONSE(404, "Not found");
	}

	return false;
}

/*
 * file <ContentReference>
 * send the given file down client_sock
 * ContentReference should be absolute, ie start with "~//"
 */

bool
cmd_file(struct listen_data *listen_data, FILE *client, int argc, char *argv[])
{
	char *filename;
	FILE *file;
	long size;
	char hdr[64];
	long left;
	size_t nread;
	char buff[1024 * 8];

	CHECK_USAGE(2, "file <ContentReference>");

	if((filename = external_filename(listen_data, argv[1])) == NULL)
	{
		SEND_RESPONSE(500, "Invalid ContentReference");
		return false;
	}

	if((file = fopen(filename, "r")) == NULL)
	{
		SEND_RESPONSE(404, "Not found");
		return false;
	}

	/* find the file length */
	if(fseek(file, 0, SEEK_END) < 0
	|| (size = ftell(file)) < 0)
	{
		SEND_RESPONSE(500, "Error reading file");
		return false;
	}
	rewind(file);

	SEND_RESPONSE(200, "OK");

	/* send the file length */
	snprintf(hdr, sizeof(hdr), "Length %ld\n", size);
	fputs(hdr, client);

	/* send the file contents */
	left = size;
	while(left > 0)
	{
		nread = fread(buff, 1, sizeof(buff), file);
		fwrite(buff, 1, nread, client);
		left -= nread;
	}

	fclose(file);

	return false;
}

/*
 * help
 */

bool
cmd_help(struct listen_data *listen_data, FILE *client, int argc, char *argv[])
{
	int i;
	char name_args[64];
	char help_line[128];

	SEND_RESPONSE(200, "OK");

	for(i=0; command[i].name != NULL; i++)
	{
		snprintf(name_args, sizeof(name_args), "%s %s", command[i].name, command[i].args);
		snprintf(help_line, sizeof(help_line), "%-30s %s\n", name_args, command[i].help);
		fputs(help_line, client);
	}

	return false;
}

/*
 * quit
 */

bool
cmd_quit(struct listen_data *listen_data, FILE *client, int argc, char *argv[])
{
	return true;
}

/*
 * return a filename that can be used to load the given ContentReference from the filesystem
 * returns a static string that will be overwritten by the next call to this routine
 * returns NULL if the ContentReference is invalid (does not start with ~// or has too many .. components)
 */

static char _external[PATH_MAX];

char *
external_filename(struct listen_data *listen_data, char *cref)
{
	char *canon_cref;

	/* is ContentReference absolute */
	if(strlen(cref) < 3 || strncmp(cref, "~//", 3) != 0)
		return NULL;

	/* strip off the ~// prefix, and canonicalise the reference */
	canon_cref = canonical_filename(cref + 3);

	/* if the canonical name starts with "../", it is invalid */
	if(strcmp(canon_cref, "..") == 0 || strncmp(canon_cref, "../", 3) == 0)
		return NULL;

	/* create the carousel filename, ie prepend the servive gateway directory */
	snprintf(_external, sizeof(_external), "%s/%u/%s", SERVICES_DIR, listen_data->carousel->service_id, canon_cref);

	return _external;
}

/*
 * return a string that recursively removes all sequences of the form '/x/../' in path
 * returns a static string that will be overwritten by the next call to this routine
 */

static char _canon[PATH_MAX];

char *
canonical_filename(char *path)
{
	char *start;
	char *slash;
	size_t len;

	/* copy path into the output buffer */
	strncpy(_canon, path, sizeof(_canon));
	/* just in case */
	_canon[sizeof(_canon)-1] = '\0';

	/* keep removing "/x/../" until there are none left */
	start = _canon;
	while(true)
	{
		/* find the start of the first path component that is not "../" */
		while(strncmp(start, "../", 3) == 0)
			start += 3;
		/* find the next slash in the path */
		slash = start;
		while(*slash != '/' && *slash != '\0')
			slash ++;
		/* no more slashes => nothing left to do */
		if(*slash == '\0')
			return _canon;
		/* if this component is empty (ie ./ or /) remove it */
		if(strncmp(start, "./", 2) == 0 || *start == '/')
		{
			/* include \0 terminator */
			len = strlen(slash + 1) + 1;
			memmove(start, slash + 1, len);
			/* restart the search */
			start = _canon;
		}
		/* if the next path component is "../", eat this one */
		else if(strncmp(slash, "/../", 4) == 0)
		{
			/* include \0 terminator */
			len = strlen(slash + 4) + 1;
			memmove(start, slash + 4, len);
			/* restart the search */
			start = _canon;
		}
		else
		{
			/* move to the next component */
			start = slash + 1;
		}
	}

	/* not reached */
	return NULL;
}

