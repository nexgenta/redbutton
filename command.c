/*
 * command.c
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "command.h"
#include "fs.h"
#include "utils.h"

/* max number of args that can be passed to a command (arbitrary) */
#define ARGV_MAX	10

/* the commands */
bool cmd_check(struct listen_data *, int, int, char **);
bool cmd_file(struct listen_data *, int, int, char **);
bool cmd_help(struct listen_data *, int, int, char **);
bool cmd_quit(struct listen_data *, int, int, char **);

static struct
{
	char *name;
	char *args;
	bool (*proc)(struct listen_data *, int, int, char **);
	char *help;
} command[] =
{
	{ "check", "<ContentReference>",	cmd_check,	"Check if the given file exists on the carousel" },
	{ "exit", "",				cmd_quit,	"Kill the programme" },
	{ "file", "<ContentReference>",		cmd_file,	"Retrieve the given file from the carousel" },
	{ "help", "",				cmd_help,	"List available commands" },
	{ "quit", "",				cmd_quit,	"Kill the programme" },
	{ NULL, NULL, NULL, NULL }
};

/* send an OK/error code etc response down client_sock */
#define SEND_RESPONSE(RC, MESSAGE)	write_string(client_sock, #RC " " MESSAGE "\n")

/* internal routines */
char *external_filename(struct listen_data *, char *);
char *canonical_filename(char *);

/*
 * process the given command
 * return true if we should quit the programme
 */

bool
process_command(struct listen_data *listen_data, int client_sock, char *cmd)
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
			return (command[i].proc)(listen_data, client_sock, argc, argv);
	}

	SEND_RESPONSE(500, "Unrecognised command");

	return false;
}

/*
 * the commands
 * listen_data is global data needed by listener commands
 * client_sock is where any response data should go
 * argc is the number of arguments passed to it
 * argv[0] is the command name (or the abreviation the user used)
 * return true if we should quit the programme
 */

#define CHECK_USAGE(ARGC, SYNTAX)		\
if(argc != ARGC)				\
{						\
	SEND_RESPONSE(500, "Syntax: " SYNTAX);	\
	return false;				\
}

/*
 * check <ContentReference>
 * check if the given file is on the carousel
 * ContentReference should be absolute, ie start with "~//"
 */

bool
cmd_check(struct listen_data *listen_data, int client_sock, int argc, char *argv[])
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
cmd_file(struct listen_data *listen_data, int client_sock, int argc, char *argv[])
{
	char *filename;
	FILE *file;
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

	SEND_RESPONSE(200, "OK");

	/* send the file contents */
	do
	{
		nread = fread(buff, 1, sizeof(buff), file);
		write_all(client_sock, buff, nread);
	}
	while(nread == sizeof(buff));

	fclose(file);

	return false;
}

/*
 * help
 */

bool
cmd_help(struct listen_data *listen_data, int client_sock, int argc, char *argv[])
{
	int i;
	char name_args[64];
	char help_line[128];

	SEND_RESPONSE(200, "OK");

	for(i=0; command[i].name != NULL; i++)
	{
		snprintf(name_args, sizeof(name_args), "%s %s", command[i].name, command[i].args);
		snprintf(help_line, sizeof(help_line), "%-30s %s\n", name_args, command[i].help);
		write_string(client_sock, help_line);
	}

	return false;
}

/*
 * quit
 */

bool
cmd_quit(struct listen_data *listen_data, int client_sock, int argc, char *argv[])
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
		/* if the next path component is "../", eat the previous one */
		if(strncmp(slash, "/../", 4) == 0)
		{
			/* include \0 terminator */
			len = strlen(start) + 1;
			memmove(start, slash + 4, len - ((slash - start) + 4));
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

