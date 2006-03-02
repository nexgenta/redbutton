/*
 * utils.c
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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "utils.h"

/*
 * write the given string (ie upto \0) to the given fd
 */

void
write_string(int fd, const char *str)
{
	write_all(fd, str, strlen(str));

	return;
}

/*
 * guarantee writing count bytes from buf to fd
 * W. Richard Stevens
 */

void
write_all(int fd, const void *buf, size_t count)
{
	size_t nwritten;
	const char *buf_ptr;

	buf_ptr = buf;
	while(count > 0)
	{
		if((nwritten = write(fd, buf_ptr, count)) < 0)
		{
			if(errno == EINTR || errno == EAGAIN)
				nwritten = 0;
			else
				fatal("write: %s\n", strerror(errno));
		}
		count -= nwritten;
		buf_ptr += nwritten;
	}

	return;
}

/*
 * returns 15 for 'f' etc
 */

unsigned int
char2hex(unsigned char c)
{
	if(!isxdigit(c))
		return 0;
	else if(c >= '0' && c <= '9')
		return c - '0';
	else
		return 10 + (tolower(c) - 'a');
}

/*
 * returns the next UTF8 character in the given text
 * size should be the amount of data available in text
 * sets *used to the number of bytes in the UTF8 character
 * gives up if the char is more than 4 bytes long
 */

int
next_utf8(unsigned char *text, int size, int *used)
{
	if(size >= 1 && (text[0] & 0x80) == 0)
	{
		*used = 1;
		return text[0];
	}
	else if(size >= 2 && (text[0] & 0xe0) == 0xc0)
	{
		*used = 2;
		return ((text[0] & 0x1f) << 6) + (text[1] & 0x3f);
	}
	else if(size >= 3 && (text[0] & 0xf0) == 0xe0)
	{
		*used = 3;
		return ((text[0] & 0x0f) << 12) + ((text[1] & 0x3f) << 6) + (text[2] & 0x3f);
	}
	else if(size >= 4 && (text[0] & 0xf8) == 0xf0)
	{
		*used = 4;
		return ((text[0] & 0x07) << 18) + ((text[1] & 0x3f) << 12) + ((text[2] & 0x3f) << 6) + (text[3] & 0x3f);
	}
	else if(size > 0)
	{
		/* error, return the next char */
		*used = 1;
		return text[0];
	}
	else
	{
		*used = 0;
		return 0;
	}
}

/*
 * I don't want to double the size of my code just to deal with malloc failures
 * if you've run out of memory you're fscked anyway, me trying to recover is not gonna help...
 */

#ifdef DEBUG_ALLOC
static int _nallocs = 0;
#endif

void *
safe_malloc(size_t nbytes)
{
	void *buf;

#ifdef DEBUG_ALLOC
	_nallocs ++;
	fprintf(stderr, "safe_malloc: %d\n", _nallocs);
#endif

	if((buf = malloc(nbytes)) == NULL)
		fatal("Out of memory");

	return buf;
}

/*
 * safe_realloc(NULL, n) == safe_malloc(n)
 * safe_realloc(x, 0) == safe_free(x) and returns NULL
 */

void *
safe_realloc(void *oldbuf, size_t nbytes)
{
	void *newbuf;

	if(nbytes == 0)
	{
		safe_free(oldbuf);
		return NULL;
	}

	if(oldbuf == NULL)
		return safe_malloc(nbytes);

	if((newbuf = realloc(oldbuf, nbytes)) == NULL)
		fatal("Out of memory");

	return newbuf;
}

/*
 * safe_free(NULL) is okay
 */

void
safe_free(void *buf)
{
	if(buf != NULL)
	{
		free(buf);
#ifdef DEBUG_ALLOC
		_nallocs--;
		fprintf(stderr, "safe_free: %d\n", _nallocs);
#endif
	}

	return;
}

void
error(char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	vprintf(message, ap);
	printf("\n");
	va_end(ap);

	return;
}

void
fatal(char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	vprintf(message, ap);
	printf("\n");
	va_end(ap);

	exit(EXIT_FAILURE);
}

