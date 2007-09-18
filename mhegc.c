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
#include <strings.h>

#include "parser.h"
#include "asn1tag.h"
#include "utils.h"

void print_node(struct node *, unsigned int);
void print_indent(unsigned int);

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

	if(optind == argc - 1)
		verbose("Parsing '%s':\n", argv[optind]);
	else
		verbose("Parsing stdin:\n");

	bzero(&asn1obj, sizeof(struct node));
	asn1obj.asn1tag = ASN1TAG_SYNTHETIC;
	parse_InterchangedObject(&asn1obj);

	if(next_token())
		parse_error("Unexpected text after InterchangedObject");

	/* assert */
	if(asn1obj.siblings != NULL)
		fatal("Top level object has siblings");

	if(_verbose)
	{
		verbose("\nASN1 object tree:\n");
		print_node(&asn1obj, 0);
	}

	return EXIT_SUCCESS;
}

/*
 * verbose functions send output to stderr so error messages get interleaved correctly
 */

void
print_node(struct node *n, unsigned int indent)
{
	bool show_node;
	bool show_kids;
	struct node *kid;

	/* only show synthetic nodes if -vv was given on the cmd line */
	show_node = (!is_synthetic(n->asn1tag) || _verbose > 1);

	/* only show non-synthetic children, unless -vv was given */
	show_kids = (has_real_children(n) || _verbose > 1);

	if(show_node)
	{
		print_indent(indent);
		fprintf(stderr, "[%s %d]\n", asn1class_name(n->asn1class), n->asn1tag);
		if(show_kids)
		{
			print_indent(indent);
			fprintf(stderr, "{\n");
			indent ++;
		}
	}

	if(show_kids)
	{
		for(kid=n->children; kid; kid=kid->siblings)
			print_node(kid, indent);
	}

	if(show_node && show_kids)
	{
		indent --;
		print_indent(indent);
		fprintf(stderr, "}\n");
	}

	return;
}

void
print_indent(unsigned int indent)
{
	while(indent > 0)
	{
		fprintf(stderr, "    ");
		indent --;
	}

	return;
}

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
