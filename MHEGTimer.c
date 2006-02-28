/*
 * MHEGTimer.c
 */

#include "MHEGEngine.h"
#include "MHEGTimer.h"
#include "GroupClass.h"

/*
 * timer callback
 */

typedef struct
{
	ExternalReference *ref;		/* object that contains the timer */
	int id;				/* timer ID */
	void *fired_data;		/* passed onto GroupClass_timerFired after the event has been generated */
} TimerCBData;

static void
timer_cb(XtPointer usr_data, XtIntervalId *id)
{
	TimerCBData *data = (TimerCBData *) usr_data;
	EventData event_data;

	/* generate a TimerFired event */
	event_data.choice = EventData_integer;
	event_data.u.integer = data->id;
	MHEGEngine_generateAsyncEvent(data->ref, EventType_timer_fired, &event_data);

	/* let the object do any additional cleaning up */
	GroupClass_timerFired(data->ref, data->id, *id, data->fired_data);

	safe_free(data);

	/* process the async event we generated */
	MHEGEngine_processMHEGEvents();

	return;
}

/*
 * generate a TimerFired event after interval milliseconds
 * the source of the event will be the given ExternalReference
 * the event data will be the given timer ID
 * also calls GroupClass_timerFired after generating the event and passes fired_data to it
 */

MHEGTimer
MHEGTimer_addGroupClassTimer(unsigned int interval, ExternalReference *ref, int id, void *fired_data)
{
	MHEGTimer xtid;
	TimerCBData *data = safe_malloc(sizeof(TimerCBData));

	data->ref = ref;
	data->id = id;
	data->fired_data = fired_data;

	xtid = XtAppAddTimeOut(MHEGEngine_getDisplay()->app, interval, timer_cb, (XtPointer) data);

	return xtid;
}

void
MHEGTimer_removeGroupClassTimer(MHEGTimer id)
{
	XtRemoveTimeOut(id);
}

/*
 * returns the number of milliseconds between t1 and t0
 */

int
time_diff(struct timeval *t1, struct timeval *t0)
{
	return ((t1->tv_sec - t0->tv_sec) * 1000) + ((t1->tv_usec - t0->tv_usec) / 1000);
}

