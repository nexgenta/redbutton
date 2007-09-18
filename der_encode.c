/*
 * der_encode.c
 */

#include <stdbool.h>
#include <string.h>

#include "der_encode.h"
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

	/* convert the STRING, QPRINTABLE or BASE64 data to an OctetString */
	if(*str == '"')
		convert_STRING(out, len, str);
	else if(*str == '\'')
		convert_QPRINTABLE(out, len, str);
	else if(*str == '`')
		convert_BASE64(out, len, str);
	else
		fatal("der_encode_OctetString: str[0]=0x%02x", *str);

	return;
}

/*
 * string is enclosed in "
 * " and \ within the string are encoded as \" and \\
 */

void
convert_STRING(unsigned char **out, unsigned int *len, const char *str)
{
	unsigned char *p;

	/* max size it could be */
	*out = safe_malloc(strlen(str));

	/* skip the initial " */
	str ++;
	p = *out;
	while(*str != '"')
	{
		if(*str != '\\')
		{
			*p = *str;
			p ++;
			str ++;
		}
		else if(*(str + 1) == '"')
		{
			*p = '"';
			p ++;
			str += 2;
		}
		else if(*(str + 1) == '\\')
		{
			*p = '\\';
			p ++;
			str += 2;
		}
		else
		{
			fatal("Invalid escape sequence in STRING: %s", str - 1);
		}
	}

	/* check we got to the closing quote */
	if(*(str + 1) != '\0')
		fatal("Unquoted \" in STRING: %s", str - 1);

	/* return the length (note: no \0 terminator) */
	*len = (p - *out);

	return;
}

void
convert_QPRINTABLE(unsigned char **out, unsigned int *len, const char *str)
{
}

void
convert_BASE64(unsigned char **out, unsigned int *len, const char *str)
{
}

