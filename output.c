/*
 * output.c
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

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "utils.h"

void print_newline(FILE *);
void print_token(FILE *, char *);

int indent = 0;
bool newline = true;

void
output_token(FILE *out, char *tok)
{
	/* assert */
	if(tok[0] == '\0')
		fatal("output_token: 0 length token");

	if(tok[0] == '{' && tok[1] == ':')
	{
		print_newline(out);
		print_token(out, tok);
		indent ++;
	}
	else if(tok[0] == '}')
	{
		/* assert */
		if(indent == 0)
			fatal("output_token: unexpected '}'");
		indent --;
		print_newline(out);
		print_token(out, tok);
	}
	else if(tok[0] == ':' && strcmp(tok, ":ContentRef") != 0)
	{
		print_newline(out);
		print_token(out, tok);
	}
	else
	{
		print_token(out, tok);
	}

	return;
}

void
print_newline(FILE *out)
{
	if(!newline)
		fprintf(out, "\n");

	newline = true;

	return;
}

void
print_token(FILE *out, char *tok)
{
	int i = indent;

	if(newline)
	{
		while(i > 0)
		{
			fprintf(out, "\t");
			i --;
		}
		newline = false;
	}
	else
	{
		fprintf(out, " ");
	}

	fprintf(out, "%s", tok);

	return;
}
