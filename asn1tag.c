/*
 * asn1tag.c
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
	return (asn1tag == ASN1TAG_SYNTHETIC || asn1tag == ASN1TAG_CHOICE);
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
