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

