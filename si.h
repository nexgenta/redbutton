/*
 * si.h
 */

#ifndef __SI_H__
#define __SI_H__

#include "der_decode.h"

int si_get_index(OctetString *);
bool si_tune_index(int);

unsigned int si_get_service_id(OctetString *);

void si_free(void);

#endif	/* __SI_H__ */
