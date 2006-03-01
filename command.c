/*
 * command.c
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "command.h"
#include "utils.h"

/* max number of args that can be passed to a command (arbitrary) */
#define ARGV_MAX	10

/* the commands */
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
	{ "exit", "",			cmd_quit,	"Kill the programme" },
	{ "file", "<ContentReference>",	cmd_file,	"Retrieve the given file from the carousel" },
	{ "help", "",			cmd_help,	"List available commands" },
	{ "quit", "",			cmd_quit,	"Kill the programme" },
	{ NULL, NULL, NULL, NULL }
};

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

	write_string(client_sock, "500 Unrecognised command\n");

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

	if(argc != 2)
	{
		write_string(client_sock, "500 Syntax: file <ContentReference>\n");
		return false;
	}

	filename = argv[1];

	/* is ContentReference absolute */
	if(strlen(filename) < 3 || strncmp(filename, "~//", 3) != 0)
	{
		write_string(client_sock, "500 ContentReference is not absolute\n");
		return false;
	}

	/* strip off the ~// prefix */
	filename += 3;

	if((file = fopen(filename, "r")) == NULL)
	{
		write_string(client_sock, "404 Not found\n");
		return false;
	}

	write_string(client_sock, "200 OK\n");

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

	write_string(client_sock, "200 OK\n");

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

