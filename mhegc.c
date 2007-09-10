/*
 * mhegc.c
 */

/*
 * Copyright (C) 2007, Simon Kilvington
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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "parser.h"

void usage(char *);

static int _verbose = 0;

int
main(int argc, char *argv[])
{
	char *prog_name = argv[0];
	int arg;
	struct node asn1obj;

	while((arg = getopt(argc, argv, "v")) != EOF)
	{
		switch(arg)
		{
		case 'v':
			_verbose ++;
			break;

		default:
			usage(prog_name);
			break;
		}
	}

	/*
	 * a single param is the name of a source file
	 * default is to read from stdin
	 */
	if(optind == argc - 1)
		set_input_file(argv[optind]);
	else if(optind != argc)
		usage(prog_name);

	parse_InterchangedObject(&asn1obj);

	if(next_token())
		parse_error("Unexpected text after InterchangedObject");

	return EXIT_SUCCESS;
}

/*
 * verbose functions send output to stderr so error messages get interleaved correctly
 */

void
verbose(const char *fmt, ...)
{
	va_list ap;

	if(_verbose)
	{
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}

	return;
}

void
vverbose(const char *fmt, ...)
{
	va_list ap;

	if(_verbose > 1)
	{
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}

	return;
}

void
usage(char *prog_name)
{
	fprintf(stderr, "Usage: %s [-vv] [<input_file>]\n", prog_name);

	exit(EXIT_FAILURE);
}
