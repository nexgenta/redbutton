/*
 * rb-browser [-v] [-f] [-o <video_output_method>] [-k <keymap_file>] [-t <timeout>] [-r] <service_gateway>
 *
 * -v is verbose/debug mode
 * -f is full screen, otherwise it uses a window
 * -o allows you to choose a video output method if the default is not supported/too slow on your graphics card
 * (do 'rb-browser -o' for a list of available methods)
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
	MHEGEngineOptions opts;
	int arg;
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

	/* default options */
	bzero(&opts, sizeof(MHEGEngineOptions));
	opts.remote = false;
	opts.srg_loc = NULL;	/* must be given on cmd line */
	opts.verbose = 0;
	opts.fullscreen = false;
	opts.vo_method = NULL;
	opts.timeout = MISSING_CONTENT_TIMEOUT;
	opts.keymap = NULL;

	while((arg = getopt(argc, argv, "rvfo:k:t:")) != EOF)
	{
		switch(arg)
		{
		case 'r':
			opts.remote = true;
			break;

		case 'v':
			opts.verbose ++;
			break;

		case 'f':
			opts.fullscreen = true;
			break;

		case 'o':
			opts.vo_method = optarg;
			break;

		case 'k':
			opts.keymap = optarg;
			break;

		case 't':
			opts.timeout = strtoul(optarg, NULL, 0);
			break;

		default:
			usage(prog_name);
			break;
		}
	}

	if(optind != argc - 1)
		usage(prog_name);

	opts.srg_loc = argv[optind];

	/* chop off any trailing / chars for local directory name */
	if(!opts.remote)
	{
		last = strlen(opts.srg_loc) - 1;
		while(last > 0 && opts.srg_loc[last] == '/')
			opts.srg_loc[last--] = '\0';
	}

	MHEGEngine_init(&opts);

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
		error("Unable to find boot object in service gateway '%s'", opts.srg_loc);
		rc = EXIT_FAILURE;
	}

	/* clean up */
	MHEGEngine_fini();

	return rc;
}

void
usage(char *prog_name)
{
	fatal("Usage: %s "
		"[-v] "
		"[-f] "
		"[-o <video_output_method>] "
		"[-k <keymap_file>] "
		"[-t <timeout>] "
		"[-r] "
		"<service_gateway>\n\n"
		"%s",
		prog_name, MHEGVideoOutputMethod_getUsage());
}

