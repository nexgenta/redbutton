/*
 * GroupClass.h
 */

#ifndef __GROUPCLASS_H__
#define __GROUPCLASS_H__

#include <sys/time.h>

#include "ISO13522-MHEG-5.h"

void GroupClass_SetTimer(ExternalReference *, LIST_OF(Timer) **, struct timeval *, SetTimer *, OctetString *);
void GroupClass_timerFired(ExternalReference *, int, MHEGTimer, LIST_OF(Timer) **);

void GroupClass_freeTimers(LIST_OF(Timer) **);

#endif	/* __GROUPCLASS_H__ */

