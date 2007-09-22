/*
 * der_encode.c
 */

#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "der_encode.h"
#include "utils.h"

void
der_encode_BOOLEAN(unsigned char **out, unsigned int *len, bool val)
{
	/* assert */
	if(*out != NULL || *len != 0)
		fatal("der_encode_BOOLEAN: length already %u", *len);

	*len = 1;
	*out = safe_malloc(1);
	(*out)[0] = val ? 1 : 0;

	return;
}

void
der_encode_INTEGER(unsigned char **out, unsigned int *len, int val)
{
	unsigned int shifted;
	unsigned int i;
	unsigned int uval;

	/* assert */
	if(*out != NULL || *len != 0)
		fatal("der_encode_INTEGER: length already %u", *len);

	/* is it +ve or -ve */
	if(val >= 0)
	{
		/*
		 * work out how many bytes we need to store 'val'
		 * ints in DER are signed, so first byte we store must be <= 127
		 * we add a leading 0 if first byte is > 127
		 */
		shifted = val;
		*len = 1;
		while(shifted > 127)
		{
			(*len) ++;
			shifted >>= 8;
		}
		*out = safe_malloc(*len);
		/* big endian */
		for(i=1; i<=(*len); i++)
			(*out)[i - 1] = (val >> (((*len) - i) * 8)) & 0xff;
	}
	else
	{
		/* work with an unsigned value */
		uval = (unsigned int) val;
		/*
		 * use as few bytes as possible
		 * ie chop off leading 0xff's while the next byte has its top bit set
		 */
		*len = sizeof(unsigned int);
		while((*len) > 1
		   && ((uval >> (((*len) - 1) * 8)) & 0xff) == 0xff
		   && ((uval >> (((*len) - 2) * 8)) & 0x80) == 0x80)
		{
			(*len) --;
		}
		*out = safe_malloc(*len);
		/* big endian */
		for(i=1; i<=(*len); i++)
			(*out)[i - 1] = (uval >> (((*len) - i) * 8)) & 0xff;
	}

	return;
}

void
der_encode_OctetString(unsigned char **out, unsigned int *len, const unsigned char *str)
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
 * contains chars 0x20 to 0x7e
 * " and \ within the string are encoded as \" and \\
 */

void
convert_STRING(unsigned char **out, unsigned int *len, const unsigned char *str)
{
	const unsigned char *whole_str = str;
	unsigned char *p;

	/* max size it could be */
	*out = safe_malloc(strlen(str));

	/* skip the initial " */
	str ++;
	p = *out;
	while(*str != '"')
	{
		if(*str < 0x20 || *str > 0x7e)
		{
			fatal("Invalid character (0x%02x) in STRING: %s", *str, whole_str);
		}
		else if(*str != '\\')
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
			/* TODO: show the line number */
			fatal("Invalid escape sequence in STRING: %s", whole_str);
		}
	}

	/* check we got to the closing quote */
	if(*(str + 1) != '\0')
		fatal("Unquoted \" in STRING: %s", whole_str);

	/* return the length (note: no \0 terminator) */
	*len = (p - *out);

	return;
}

/*
 * string is enclosed in '
 * encoded as specified in RFC 1521 (MIME)
 * in addition, ' is encoded as =27
 * and line breaks do not need to be converted to CRLF
 */

void
convert_QPRINTABLE(unsigned char **out, unsigned int *len, const unsigned char *str)
{
	const unsigned char *whole_str = str;
	unsigned char *p;

	/* max size it could be */
	*out = safe_malloc(strlen(str));

	/* skip the initial ' */
	str ++;
	p = *out;
	while(*str != '\'')
	{
		if(*str != '=')
		{
			*p = *str;
			p ++;
			str ++;
		}
		else if(isxdigit(*(str + 1)) && isxdigit(*(str + 2)))
		{
			*p = char2hex(*(str + 1)) << 4 | char2hex(*(str + 2));
			p ++;
			str += 3;
		}
		else
		{
			/* TODO: show the line number */
			fatal("Invalid escape sequence in QPRINTABLE: %s", whole_str);
		}
	}

	/* check we got to the closing quote */
	if(*(str + 1) != '\0')
		fatal("Unquoted ' in QPRINTABLE: %s", whole_str);

	/* return the length (note: no \0 terminator) */
	*len = (p - *out);

	return;
}

void
convert_BASE64(unsigned char **out, unsigned int *len, const unsigned char *str)
{
/* TODO */
printf("TODO: convert_BASE64\n");
}

/*
 * recursively generate the DER tag/length header for the tree of nodes
 */

unsigned int
gen_der_header(struct node *n)
{
	unsigned int val_length;
	struct node *kid;

	/* if it is a constructed type, sum the length of its children */
	if(n->children)
	{
		/* assert */
		if(n->length != 0)
			fatal("Constructed type [%s %u] has a value", asn1class_name(n->asn1class), n->asn1tag);
		/* recursively generate the DER headers for our child nodes */
		val_length = 0;
		for(kid=n->children; kid; kid=kid->siblings)
			val_length += gen_der_header(kid);
	}
	else
	{
		val_length = n->length;
	}

	/* we don't need a header if we are a synthetic object */
	if(!is_synthetic(n->asn1tag))
	{
printf("TODO: gen_der_header: tag=%u class=%s len=%u\n", n->asn1tag, asn1class_name(n->asn1class), val_length);
	}

	return n->hdr_length + val_length;
}

