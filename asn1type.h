/*
 * asn1type.h
 */

#ifndef __ASN1TYPE_H__
#define __ASN1TYPE_H__

enum asn1type
{
	ASN1TYPE_UNKNOWN,
	ASN1TYPE_CHOICE,
	ASN1TYPE_ENUMERATED,
	ASN1TYPE_SET,
	ASN1TYPE_SEQUENCE
};

enum asn1type asn1type(char *);

#endif	/* __ASN1TYPE_H__ */
