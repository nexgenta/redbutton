/*
 * AudioClass.c
 */

#include "MHEGEngine.h"
#include "AudioClass.h"
#include "RootClass.h"
#include "ExternalReference.h"

void
default_AudioClassInstanceVars(AudioClass *t, AudioClassInstanceVars *v)
{
	bzero(v, sizeof(AudioClassInstanceVars));

	v->Volume = t->original_volume;

	return;
}

void
free_AudioClassInstanceVars(AudioClassInstanceVars *v)
{
	return;
}

void
AudioClass_Preparation(AudioClass *t)
{
	verbose("AudioClass: %s; Preparation", ExternalReference_name(&t->rootClass.inst.ref));

	if(!RootClass_Preparation(&t->rootClass))
		return;

	default_AudioClassInstanceVars(t, &t->inst);

	return;
}

void
AudioClass_Activation(AudioClass *t)
{
	verbose("AudioClass: %s; Activation", ExternalReference_name(&t->rootClass.inst.ref));

	/* is it already activated */
	if(t->rootClass.inst.RunningStatus)
		return;

	/* has it been prepared yet */
	if(!t->rootClass.inst.AvailabilityStatus)
	{
		/* generates an IsAvailable event */
		AudioClass_Preparation(t);
	}

	/* set RunningStatus */
	t->rootClass.inst.RunningStatus = true;

	/* generate IsRunning event */
	MHEGEngine_generateEvent(&t->rootClass.inst.ref, EventType_is_running, NULL);

	return;
}

void
AudioClass_Deactivation(AudioClass *t)
{
	verbose("AudioClass: %s; Deactivation", ExternalReference_name(&t->rootClass.inst.ref));

	RootClass_Deactivation(&t->rootClass);

	return;
}

void
AudioClass_Destruction(AudioClass *t)
{
	verbose("AudioClass: %s; Destruction", ExternalReference_name(&t->rootClass.inst.ref));

	if(!RootClass_Destruction(&t->rootClass))
		return;

	free_AudioClassInstanceVars(&t->inst);

	return;
}

void
AudioClass_SetVolume(AudioClass *t, SetVolume *params, OctetString *caller_gid)
{
	verbose("AudioClass: %s; SetVolume", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: AudioClass_SetVolume not yet implemented\n");
	return;
}

void
AudioClass_GetVolume(AudioClass *t, GetVolume *params, OctetString *caller_gid)
{
	verbose("AudioClass: %s; GetVolume", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: AudioClass_GetVolume not yet implemented\n");
	return;
}

