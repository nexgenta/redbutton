/*
 * rb-download [-d <demux_device>] [-b <base_dir>] [-t <timeout>] [-c <carousel_id>] [<service_id>]
 *
 * Download the DVB Object Carousel for the given channel onto the local hard disc
 * files will be stored under the current dir if no -b option is given
 *
 * if no service_id is given, a list of possible channels (and their service_id) is printed
 * the carousel ID is normally read from the PMT, use -c to explicitly set it
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
 * Copyright (C) 2005, Simon Kilvington
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
#include <errno.h>

#include "list.h"
#include "findmheg.h"
#include "carousel.h"
#include "utils.h"

/* DVB demux device */
#define DEFAULT_DEVICE	"/dev/dvb/adapter0/demux0"

/* seconds before we assume no DSMCC data is available on this PID */
#define DEFAULT_TIMEOUT	10

void usage(char *);

int
main(int argc, char *argv[])
{
	char *prog_name = argv[0];
	char *device = DEFAULT_DEVICE;
	unsigned int timeout = DEFAULT_TIMEOUT;
	int carousel_id = -1;
	uint16_t service_id;
	struct carousel *car;
	int arg;

	while((arg = getopt(argc, argv, "d:b:t:c:")) != EOF)
	{
		switch(arg)
		{
		case 'd':
			device = optarg;
			break;

		case 'b':
			if(chdir(optarg) < 0)
				fatal("Unable to cd to '%s': %s", optarg, strerror(errno));
			break;

		case 't':
			timeout = strtoul(optarg, NULL, 0);
			break;

		case 'c':
			carousel_id = strtoul(optarg, NULL, 0);
			break;

		default:
			usage(prog_name);
			break;
		}
	}

	if(argc == optind)
	{
		list_channels(device, timeout);
	}
	else if(argc - optind == 1)
	{
		service_id = strtoul(argv[optind], NULL, 0);
		car = find_mheg(device, timeout, service_id, carousel_id);
		printf("Carousel ID=%u\n", car->carousel_id);
		load_carousel(car);
	}
	else
	{
		usage(prog_name);
	}

	return EXIT_SUCCESS;
}

void
usage(char *prog_name)
{
	fatal("Usage: %s [-d <demux_device>] [-b <base_dir>] [-t <timeout>] [-c carousel_id] [<service_id>]", prog_name);
}

