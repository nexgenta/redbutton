/*
 * StreamClass.c
 */

#include "MHEGEngine.h"
#include "ISO13522-MHEG-5.h"
#include "StreamComponent.h"
#include "ExternalReference.h"
#include "ContentBody.h"
#include "utils.h"

void
default_StreamClassInstanceVars(StreamClass *t, StreamClassInstanceVars *v)
{
	bzero(v, sizeof(StreamClassInstanceVars));

	v->Speed.numerator = 1;
	v->Speed.have_denominator = true;
	v->Speed.demoninator = 1;

	v->CounterPosition = 0;
	v->CounterEndPosition = -1;

	v->CounterTriggers = NULL;

	MHEGStreamPlayer_init(&v->player);

	return;
}

void
free_StreamClassInstanceVars(StreamClassInstanceVars *v)
{
	MHEGStreamPlayer_fini(&v->player);

	LIST_FREE(&v->CounterTriggers, CounterTrigger, safe_free);

	return;
}

void
StreamClass_Preparation(StreamClass *t)
{
	LIST_TYPE(StreamComponent) *comp;

	verbose("StreamClass: %s; Preparation", ExternalReference_name(&t->rootClass.inst.ref));

	/* is it already prepared */
	if(t->rootClass.inst.AvailabilityStatus)
		return;

	default_StreamClassInstanceVars(t, &t->inst);

	/* do Activation of each initially active StreamComponent */
	comp = t->multiplex;
	while(comp)
	{
		if(StreamComponent_isInitiallyActive(&comp->item))
			StreamComponent_Activation(&comp->item);
		comp = comp->next;
	}

	/* finish the RootClass Preparation */
	t->rootClass.inst.AvailabilityStatus = true;

	/* generate IsAvailable event */
	MHEGEngine_generateEvent(&t->rootClass.inst.ref, EventType_is_available, NULL);

	/* generate an asynchronous ContentAvailable event */
	MHEGEngine_generateAsyncEvent(&t->rootClass.inst.ref, EventType_content_available, NULL);

	return;
}

void
StreamClass_Activation(StreamClass *t)
{
	LIST_TYPE(StreamComponent) *comp;
	RootClass *r;
	OctetString *service;

	verbose("StreamClass: %s; Activation", ExternalReference_name(&t->rootClass.inst.ref));

	/* RootClass Preparartion */
	/* is it already activated */
	if(t->rootClass.inst.RunningStatus)
		return;

	/* has it been prepared yet */
	if(!t->rootClass.inst.AvailabilityStatus)
	{
		/* generates an IsAvailable event */
		StreamClass_Preparation(t);
	}

	/* assume default is "rec://svc/cur", ie current channel */
	if(t->have_original_content
	&& (service = ContentBody_getReference(&t->original_content)) != NULL
	&& OctetString_strcmp(service, "rec://svc/cur") != 0)
	{
printf("TODO: StreamClass: service='%.*s'\n", service->size, service->data);
	}

	/* start playing all active StreamComponents */
	comp = t->multiplex;
	while(comp)
	{
		if((r = StreamComponent_rootClass(&comp->item)) != NULL
		&& r->inst.RunningStatus)
			StreamComponent_play(&comp->item, &t->inst.player);
		comp = comp->next;
	}
	MHEGStreamPlayer_play(&t->inst.player);

	/* multiplex is now playing */
	MHEGEngine_generateAsyncEvent(&t->rootClass.inst.ref, EventType_stream_playing, NULL);

	/* finish our Activation */
	t->rootClass.inst.RunningStatus = true;
	MHEGEngine_generateEvent(&t->rootClass.inst.ref, EventType_is_running, NULL);

	return;
}

void
StreamClass_Deactivation(StreamClass *t)
{
	LIST_TYPE(StreamComponent) *comp;
	RootClass *r;

	verbose("StreamClass: %s; Deactivation", ExternalReference_name(&t->rootClass.inst.ref));

	/* are we already deactivated */
	if(!t->rootClass.inst.RunningStatus)
		return;

	/* stop playing all active StreamComponents */
	MHEGStreamPlayer_stop(&t->inst.player);
	comp = t->multiplex;
	while(comp)
	{
		if((r = StreamComponent_rootClass(&comp->item)) != NULL
		&& r->inst.RunningStatus)
			StreamComponent_stop(&comp->item, &t->inst.player);
		comp = comp->next;
	}

	/* multiplex is now stopped */
	MHEGEngine_generateAsyncEvent(&t->rootClass.inst.ref, EventType_stream_stopped, NULL);

	/* finish our Deactivation */
	t->rootClass.inst.RunningStatus = false;
	MHEGEngine_generateEvent(&t->rootClass.inst.ref, EventType_is_stopped, NULL);

	return;
}

void
StreamClass_Destruction(StreamClass *t)
{
	LIST_TYPE(StreamComponent) *comp;
	LIST_TYPE(StreamComponent) *comp_tail;
	RootClass *r;

	verbose("StreamClass: %s; Destruction", ExternalReference_name(&t->rootClass.inst.ref));

	/* is it already destroyed */
	if(!t->rootClass.inst.AvailabilityStatus)
		return;

	/* Deactivate it if it is running */
	if(t->rootClass.inst.RunningStatus)
	{
		/* generates an IsStopped event */
		StreamClass_Deactivation(t);
	}

	/* do Destruction of all StreamComponents in the reverse order they appear in the list */
	comp = t->multiplex;
	/* find the tail */
	comp_tail = (comp != NULL) ? comp->prev : NULL;
	comp = comp_tail;
	while(comp)
	{
		/* only do Destruction if it is available */
		if((r = StreamComponent_rootClass(&comp->item)) != NULL
		&& r->inst.AvailabilityStatus)
			StreamComponent_Destruction(&comp->item);
		/* have we reached the head */
		comp = (comp->prev != comp_tail) ? comp->prev : NULL;
	}

	/* RootClass Destruction */
/* TODO caching */
	free_StreamClassInstanceVars(&t->inst);

	/* generate an IsDeleted event */
	t->rootClass.inst.AvailabilityStatus = false;
	MHEGEngine_generateEvent(&t->rootClass.inst.ref, EventType_is_deleted, NULL);

	return;
}

void
StreamClass_SetCounterTrigger(StreamClass *t, SetCounterTrigger *params, OctetString *caller_gid)
{
	verbose("StreamClass: %s; SetCounterTrigger", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: StreamClass_SetCounterTrigger not yet implemented\n");
	return;
}

void
StreamClass_SetSpeed(StreamClass *t, SetSpeed *params, OctetString *caller_gid)
{
	verbose("StreamClass: %s; SetSpeed", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: StreamClass_SetSpeed not yet implemented\n");
	return;
}

void
StreamClass_SetCounterPosition(StreamClass *t, SetCounterPosition *params, OctetString *caller_gid)
{
	verbose("StreamClass: %s; SetCounterPosition", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: StreamClass_SetCounterPosition not yet implemented\n");
	return;
}

void
StreamClass_SetCounterEndPosition(StreamClass *t, SetCounterEndPosition *params, OctetString *caller_gid)
{
	verbose("StreamClass: %s; SetCounterEndPosition", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: StreamClass_SetCounterEndPosition not yet implemented\n");
	return;
}

