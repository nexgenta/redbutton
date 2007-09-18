/*
 * der_encode.h
 */

#ifndef __DER_ENCODE_H__
#define __DER_ENCODE_H__

#include <stdbool.h>

void der_encode_BOOLEAN(unsigned char **, unsigned int *, bool);
void der_encode_INTEGER(unsigned char **, unsigned int *, int);
void der_encode_OctetString(unsigned char **, unsigned int *, const char *);

void convert_STRING(unsigned char **, unsigned int *, const char *);
void convert_QPRINTABLE(unsigned char **, unsigned int *, const char *);
void convert_BASE64(unsigned char **, unsigned int *, const char *);

#endif	/* __DER_ENCODE_H__ */

