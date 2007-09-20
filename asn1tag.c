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

/*
 * returns true if we need an explicit tag for this primitive type
 */

bool
needs_tagging(unsigned int asn1tag, unsigned int asn1class)
{
	/* only need an explicit tag if we are a choice or a constructed type */
	return (asn1tag == ASN1TAG_CHOICE)
	    || (asn1class == ASN1CLASS_UNIVERSAL && asn1tag == ASN1TAG_SEQUENCE)
	    || (asn1class == ASN1CLASS_UNIVERSAL && asn1tag == ASN1TAG_SET);
}
