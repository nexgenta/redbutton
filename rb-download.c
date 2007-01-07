/*
 * rb-download [-v] [-a <adapter>] [-b <base_dir>] [-t <timeout>] [-l[<listen-addr>]] [-c <carousel_id>] [<service_id>]
 *
 * Download the DVB Object Carousel for the given channel onto the local hard disc
 * files will be stored under the current dir if no -b option is given
 *
 * if no service_id is given, a list of possible channels (and their service_id) is printed
 * the carousel ID is normally read from the PMT, use -c to explicitly set it
 *
 * the default timeout is 10 seconds
 * if no DSMCC data is read after this time, it is assumed none is being broadcast
 *
 * the DVB devices used will be:
 * /dev/dvb/adapter0/demux0
 * /dev/dvb/adapter0/dvr0
 * use the -a option to change the adapter number (eg "-a 1" will use /dev/dvb/adapter1/demux0 etc)
 *
 * if -l is given, rb-download listens on the network for commands from a remote rb-browser
 * the default IP to listen on is 0.0.0.0 (ie all interfaces), the default TCP port is 10101
 * listen-addr should be given in the form "host:port", where host defaults to 0.0.0.0 and port defaults to 10101
 * eg, to listen on a different port, do "-l8080"
 * to only listen on the loop back, do "-l127.0.0.1" or on a different port too, do "-l127.0.0.1:8080"
 * NOTE: because -l may or may not take an argument, you must not put a space between the -l and the value
 * (otherwise, "rb-download -l 1234", is ambiguous - listen on port 1234 or use service_id 1234?)
 *
 * -v is verbose/debug mode, use more v's for more verbosity
 *
 * the file structure will be:
 * ./services/<service_id>
 * this is a symlink to the root of the carousel
 * the actual carousel files and directories are stored under:
 * ./carousels/<PID>/<CID>/
 * where <PID> is the PID the carousel was downloaded from
 * and <CID> is the Carousel ID
 */

/*
 * Copyright (C) 2005, 2006, Simon Kilvington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <netinet/in.h>

#include "list.h"
#include "findmheg.h"
#include "carousel.h"
#include "listen.h"
#include "utils.h"

/* seconds before we assume no DSMCC data is available on this PID */
#define DEFAULT_TIMEOUT	10

/* default listen address */
#define DEFAULT_LISTEN_ADDR	INADDR_ANY
#define DEFAULT_LISTEN_PORT	10101

static int _verbose = 0;

void usage(char *);

int
main(int argc, char *argv[])
{
	char *prog_name = argv[0];
	unsigned int adapter = 0;
	unsigned int timeout = DEFAULT_TIMEOUT;
	bool listen = false;
	struct listen_data listen_data;
	int carousel_id = -1;
	uint16_t service_id;
	struct carousel *car;
	int arg;

	while((arg = getopt(argc, argv, "a:b:t:l::c:v")) != EOF)
	{
		switch(arg)
		{
		case 'a':
			adapter = strtoul(optarg, NULL, 0);
			break;

		case 'b':
			if(chdir(optarg) < 0)
				fatal("Unable to cd to '%s': %s", optarg, strerror(errno));
			break;

		case 't':
			timeout = strtoul(optarg, NULL, 0);
			break;

		case 'l':
			listen = true;
			/* default values */
			listen_data.addr.sin_addr.s_addr = htonl(DEFAULT_LISTEN_ADDR);
			listen_data.addr.sin_port = htons(DEFAULT_LISTEN_PORT);
			/* optarg is NULL if no value is given, parse_addr can't fail with NULL */
			if(parse_addr(optarg, &listen_data.addr.sin_addr, &listen_data.addr.sin_port) < 0)
				fatal("Unable to resolve host %s", optarg);
			break;

		case 'c':
			carousel_id = strtoul(optarg, NULL, 0);
			break;

		case 'v':
			_verbose ++;
			break;

		default:
			usage(prog_name);
			break;
		}
	}

	if(argc == optind)
	{
		list_channels(adapter, timeout);
	}
	else if(argc - optind == 1)
	{
		service_id = strtoul(argv[optind], NULL, 0);
		car = find_mheg(adapter, timeout, service_id, carousel_id);
		verbose("Carousel ID=%u", car->carousel_id);
		verbose("Boot PID=%u", car->boot_pid);
		verbose("Video PID=%u", car->video_pid);
		verbose("Audio PID=%u", car->audio_pid);
		if(listen)
		{
			listen_data.carousel = car;
			start_listener(&listen_data);
		}
		load_carousel(car);
	}
	else
	{
		usage(prog_name);
	}

	return EXIT_SUCCESS;
}

void
verbose(char *message, ...)
{
	va_list ap;

	if(_verbose)
	{
	        va_start(ap, message);
	        vprintf(message, ap);
		printf("\n");
	        va_end(ap);
	}

	return;
}

void
vverbose(char *message, ...)
{
	va_list ap;

	if(_verbose > 1)
	{
	        va_start(ap, message);
	        vprintf(message, ap);
		printf("\n");
	        va_end(ap);
	}

	return;
}

void
vhexdump(unsigned char *data, size_t nbytes)
{
	if(_verbose > 1)
		hexdump(data, nbytes);

	return;
}

void
usage(char *prog_name)
{
	fatal("Usage: %s [-v] "
			"[-a <adapter>] "
			"[-b <base_dir>] "
			"[-t <timeout>] "
			"[-l[<listen-addr>]] "
			"[-c carousel_id] "
			"[<service_id>]", prog_name);
}

