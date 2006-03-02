/*
 * rb-browser [-v] [-f] [-k <keymap_file>] [-t <timeout>] [-r] <service_gateway>
 *
 * -v is verbose/debug mode
 * -f is full screen, otherwise it uses a window
 * -k changes the default key map to the given file
 * (use rb-keymap to generate a keymap config file)
 * -t is how long to poll for missing files before generating a ContentRefError (default 10 seconds)
 * -r means use a remote backend (rb-download running on another host), <service_gateway> should be host[:port]
 * if -r is not specified, rb-download is running on the same machine
 * and <service_gateway> should be an entry in the services directory, eg. services/4165
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "MHEGEngine.h"
#include "utils.h"

void usage(char *);

int
main(int argc, char *argv[])
{
	char *prog_name = argv[0];
	int arg;
	bool remote = false;
	int v = 0;
	bool f = false;
	unsigned int timeout = MISSING_CONTENT_TIMEOUT;
	char *keymap = NULL;
	char *srg_dir;
	size_t last;
	int rc;
	int i;
	bool found;
	OctetString boot_obj;
	/* search order for the app to boot in the Service Gateway dir */
	char *boot_order[] = { "~//a", "~//startup", NULL };

	/* we assume &struct == &struct.first_item, not sure if C guarantees it */
	ApplicationClass app;
	if(&app != (ApplicationClass *) &app.rootClass)
		fatal("%s needs to be compiled with a compiler that makes &struct == &struct.first_item", prog_name);
	/* let's be really paranoid */
	if(NULL != 0)
		fatal("%s needs to be compiled with a libc that makes NULL == 0", prog_name);

	while((arg = getopt(argc, argv, "rvfk:t:")) != EOF)
	{
		switch(arg)
		{
		case 'r':
			remote = true;
			break;

		case 'v':
			v ++;
			break;

		case 'f':
			f = true;
			break;

		case 'k':
			keymap = optarg;
			break;

		case 't':
			timeout = strtoul(optarg, NULL, 0);
			break;

		default:
			usage(prog_name);
			break;
		}
	}

	if(optind != argc - 1)
		usage(prog_name);

	srg_dir = argv[optind];

	/* chop off any trailing / chars */
	last = strlen(srg_dir) - 1;
	while(last > 0 && srg_dir[last] == '/')
		srg_dir[last--] = '\0';

	MHEGEngine_init(remote, srg_dir, v, timeout, f, keymap);

	/* search for the boot object */
	found = false;
	for(i=0; !found && boot_order[i] != NULL; i++)
	{
		boot_obj.size = strlen(boot_order[i]);
		boot_obj.data = boot_order[i];
		found = MHEGEngine_checkContentRef(&boot_obj);
	}

	if(found)
	{
		verbose("Booting '%.*s'", boot_obj.size, boot_obj.data);
		rc = MHEGEngine_run(&boot_obj);
	}
	else
	{
		error("Unable to find boot object in '%s'", srg_dir);
		rc = EXIT_FAILURE;
	}

	/* clean up */
	MHEGEngine_fini();

	return rc;
}

void
usage(char *prog_name)
{
	fatal("Usage: %s [-v] [-f] [-k <keymap_file>] [-t <timeout>] [-r] <service_gateway>", prog_name);
}

