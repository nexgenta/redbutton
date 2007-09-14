/*
 * der_encode.c
 */

#include <stdbool.h>

#include "utils.h"

void
der_encode_BOOLEAN(unsigned char **out, unsigned int *len, bool val)
{
	/* assert */
	if(*out != NULL || *len != 0)
		fatal("der_encode_BOOLEAN: length already %u", *len);

	return;
}

void
der_encode_INTEGER(unsigned char **out, unsigned int *len, int val)
{
	/* assert */
	if(*out != NULL || *len != 0)
		fatal("der_encode_INTEGER: length already %u", *len);

	return;
}

void
der_encode_OctetString(unsigned char **out, unsigned int *len, const char *str)
{
	/* assert */
	if(*out != NULL || *len != 0)
		fatal("der_encode_OctetString: length already %u", *len);

	return;
}

