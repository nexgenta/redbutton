/*
 * asn1tag.c
 */

#include "asn1tag.h"

char *
asn1class_name(unsigned int c)
{
	if(c == ASN1CLASS_UNIVERSAL)	return "UNIVERSAL";
	if(c == ASN1CLASS_APPLICATION)	return "APPLICATION";
	if(c == ASN1CLASS_CONTEXT)	return "CONTEXT";
	if(c == ASN1CLASS_PRIVATE)	return "PRIVATE";

	return "ILLEGAL-ASN1-CLASS-NUMBER";
}

bool
is_synthetic(unsigned int asn1tag)
{
/* TODO: only need one value for synthetic types (ie no need for separate ASN1TAG_CHOICE etc) */
	return (asn1tag >= ASN1TAG_SYNTHETIC);
}
