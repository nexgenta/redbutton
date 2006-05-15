/*
 * VideoClass.c
 */

#include "MHEGEngine.h"
#include "ISO13522-MHEG-5.h"
#include "RootClass.h"
#include "ExternalReference.h"
#include "ObjectReference.h"
#include "GenericInteger.h"
#include "VariableClass.h"
#include "IntegerVariableClass.h"
#include "GenericObjectReference.h"
#include "clone.h"
#include "rtti.h"

void
default_VideoClassInstanceVars(VideoClass *t, VideoClassInstanceVars *v)
{
	bzero(v, sizeof(VideoClassInstanceVars));

	/* VisibleClass */
	memcpy(&v->BoxSize, &t->original_box_size, sizeof(OriginalBoxSize));
	memcpy(&v->Position, &t->original_position, sizeof(XYPosition));
	v->have_PaletteRef = t->have_original_palette_ref;
	if(v->have_PaletteRef)
		ObjectReference_dup(&v->PaletteRef, &t->original_palette_ref);

	/* VideoClass */
	v->VideoDecodeOffset.x_position = 0;
	v->VideoDecodeOffset.y_position = 0;

	return;
}

void
free_VideoClassInstanceVars(VideoClassInstanceVars *v)
{
	if(v->have_PaletteRef)
		free_ObjectReference(&v->PaletteRef);

	return;
}

void
VideoClass_Preparation(VideoClass *t)
{
	verbose("VideoClass: %s; Preparation", ExternalReference_name(&t->rootClass.inst.ref));

	/* RootClass Preparation */
	if(!RootClass_Preparation(&t->rootClass))
		return;

	default_VideoClassInstanceVars(t, &t->inst);

	/* add it to the DisplayStack of the active application */
	MHEGEngine_addVisibleObject(&t->rootClass);

	return;
}

void
VideoClass_Activation(VideoClass *t)
{
	verbose("VideoClass: %s; Activation", ExternalReference_name(&t->rootClass.inst.ref));

	/* has it been prepared yet */
	if(!t->rootClass.inst.AvailabilityStatus)
		VideoClass_Preparation(t);

	/* has it already been activated */
	if(!RootClass_Activation(&t->rootClass))
		return;

	/* set its RunningStatus */
	t->rootClass.inst.RunningStatus = true;
	MHEGEngine_generateEvent(&t->rootClass.inst.ref, EventType_is_running, NULL);

	/* now its RunningStatus is true, get it drawn at its position in the application's DisplayStack */
	MHEGEngine_redrawArea(&t->inst.Position, &t->inst.BoxSize);

	return;
}

void
VideoClass_Deactivation(VideoClass *t)
{
	verbose("VideoClass: %s; Deactivation", ExternalReference_name(&t->rootClass.inst.ref));

	/* is it already deactivated */
	if(!RootClass_Deactivation(&t->rootClass))
		return;

	/* now its RunningStatus is false, redraw the area it covered */
	MHEGEngine_redrawArea(&t->inst.Position, &t->inst.BoxSize);

	return;
}

void
VideoClass_Destruction(VideoClass *t)
{
	verbose("VideoClass: %s; Destruction", ExternalReference_name(&t->rootClass.inst.ref));

	/* is it already destroyed */
	if(!t->rootClass.inst.AvailabilityStatus)
		return;

	/* remove it from the DisplayStack */
	MHEGEngine_removeVisibleObject(&t->rootClass);

	/* RootClass Destruction */
	/* Deactivate it if it is running */
	if(t->rootClass.inst.RunningStatus)
	{
		/* generates an IsStopped event */
		VideoClass_Deactivation(t);
	}

/* TODO caching */
	free_VideoClassInstanceVars(&t->inst);

	/* generate an IsDeleted event */
	t->rootClass.inst.AvailabilityStatus = false;
	MHEGEngine_generateEvent(&t->rootClass.inst.ref, EventType_is_deleted, NULL);

	return;
}

void
VideoClass_SetPosition(VideoClass *t, SetPosition *params, OctetString *caller_gid)
{
	XYPosition old;

	verbose("VideoClass: %s; SetPosition", ExternalReference_name(&t->rootClass.inst.ref));

	old.x_position = t->inst.Position.x_position;
	old.y_position = t->inst.Position.y_position;

	t->inst.Position.x_position = GenericInteger_getInteger(&params->new_x_position, caller_gid);
	t->inst.Position.y_position = GenericInteger_getInteger(&params->new_y_position, caller_gid);

	/* if it is active, redraw it */
	if(t->rootClass.inst.RunningStatus)
	{
		MHEGEngine_redrawArea(&old, &t->inst.BoxSize);
		MHEGEngine_redrawArea(&t->inst.Position, &t->inst.BoxSize);
	}

	return;
}

void
VideoClass_GetPosition(VideoClass *t, GetPosition *params, OctetString *caller_gid)
{
	VariableClass *var;

	verbose("VideoClass: %s; GetPosition", ExternalReference_name(&t->rootClass.inst.ref));

	/* X position */
	if((var = (VariableClass *) MHEGEngine_findObjectReference(&params->x_position_var, caller_gid)) == NULL)
		return;

	if(var->rootClass.inst.rtti != RTTI_VariableClass
	|| VariableClass_type(var) != OriginalValue_integer)
	{
		error("VideoClass: GetPosition: type mismatch");
		return;
	}

	IntegerVariableClass_setInteger(var, t->inst.Position.x_position);

	/* Y position */
	if((var = (VariableClass *) MHEGEngine_findObjectReference(&params->y_position_var, caller_gid)) == NULL)
		return;

	if(var->rootClass.inst.rtti != RTTI_VariableClass
	|| VariableClass_type(var) != OriginalValue_integer)
	{
		error("VideoClass: GetPosition: type mismatch");
		return;
	}

	IntegerVariableClass_setInteger(var, t->inst.Position.y_position);

	return;
}

void
VideoClass_SetBoxSize(VideoClass *t, SetBoxSize *params, OctetString *caller_gid)
{
	OriginalBoxSize old;

	verbose("VideoClass: %s; SetBoxSize", ExternalReference_name(&t->rootClass.inst.ref));

	old.x_length = t->inst.BoxSize.x_length;
	old.y_length = t->inst.BoxSize.y_length;

	t->inst.BoxSize.x_length = GenericInteger_getInteger(&params->x_new_box_size, caller_gid);
	t->inst.BoxSize.y_length = GenericInteger_getInteger(&params->y_new_box_size, caller_gid);

	/* if it is active, redraw it */
	if(t->rootClass.inst.RunningStatus)
	{
		MHEGEngine_redrawArea(&t->inst.Position, &old);
		MHEGEngine_redrawArea(&t->inst.Position, &t->inst.BoxSize);
	}

	return;
}

void
VideoClass_GetBoxSize(VideoClass *t, GetBoxSize *params, OctetString *caller_gid)
{
	VariableClass *var;

	verbose("VideoClass: %s; GetBoxSize", ExternalReference_name(&t->rootClass.inst.ref));

	/* width */
	if((var = (VariableClass *) MHEGEngine_findObjectReference(&params->x_box_size_var, caller_gid)) == NULL)
		return;

	if(var->rootClass.inst.rtti != RTTI_VariableClass
	|| VariableClass_type(var) != OriginalValue_integer)
	{
		error("VideoClass: GetBoxSize: type mismatch");
		return;
	}

	IntegerVariableClass_setInteger(var, t->inst.BoxSize.x_length);

	/* height */
	if((var = (VariableClass *) MHEGEngine_findObjectReference(&params->y_box_size_var, caller_gid)) == NULL)
		return;

	if(var->rootClass.inst.rtti != RTTI_VariableClass
	|| VariableClass_type(var) != OriginalValue_integer)
	{
		error("VideoClass: GetBoxSize: type mismatch");
		return;
	}

	IntegerVariableClass_setInteger(var, t->inst.BoxSize.y_length);

	return;
}

void
VideoClass_BringToFront(VideoClass *t)
{
	verbose("VideoClass: %s; BringToFront", ExternalReference_name(&t->rootClass.inst.ref));

	MHEGEngine_bringToFront(&t->rootClass);

	/* if it is active, redraw it */
	if(t->rootClass.inst.RunningStatus)
		MHEGEngine_redrawArea(&t->inst.Position, &t->inst.BoxSize);

	return;
}

void
VideoClass_SendToBack(VideoClass *t)
{
	verbose("VideoClass: %s; SendToBack", ExternalReference_name(&t->rootClass.inst.ref));

	MHEGEngine_sendToBack(&t->rootClass);

	/* if it is active, redraw it */
	if(t->rootClass.inst.RunningStatus)
		MHEGEngine_redrawArea(&t->inst.Position, &t->inst.BoxSize);

	return;
}

void
VideoClass_PutBefore(VideoClass *t, PutBefore *params, OctetString *caller_gid)
{
	ObjectReference *ref;
	RootClass *obj;

	verbose("VideoClass: %s; PutBefore", ExternalReference_name(&t->rootClass.inst.ref));

	if(((ref = GenericObjectReference_getObjectReference(&params->reference_visible, caller_gid)) != NULL)
	&& ((obj = MHEGEngine_findObjectReference(ref, caller_gid)) != NULL))
	{
		MHEGEngine_putBefore(&t->rootClass, obj);
		/* if it is active, redraw it */
		if(t->rootClass.inst.RunningStatus)
			MHEGEngine_redrawArea(&t->inst.Position, &t->inst.BoxSize);
	}

	return;
}

void
VideoClass_PutBehind(VideoClass *t, PutBehind *params, OctetString *caller_gid)
{
	ObjectReference *ref;
	RootClass *obj;

	verbose("VideoClass: %s; PutBehind", ExternalReference_name(&t->rootClass.inst.ref));

	if(((ref = GenericObjectReference_getObjectReference(&params->reference_visible, caller_gid)) != NULL)
	&& ((obj = MHEGEngine_findObjectReference(ref, caller_gid)) != NULL))
	{
		MHEGEngine_putBehind(&t->rootClass, obj);
		/* if it is active, redraw it */
		if(t->rootClass.inst.RunningStatus)
			MHEGEngine_redrawArea(&t->inst.Position, &t->inst.BoxSize);
	}

	return;
}

void
VideoClass_SetPaletteRef(VideoClass *t, SetPaletteRef *params, OctetString *caller_gid)
{
	verbose("VideoClass: %s; SetPaletteRef", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: VideoClass_SetPaletteRef not yet implemented\n");
	return;
}

void
VideoClass_SetVideoDecodeOffset(VideoClass *t, SetVideoDecodeOffset *params, OctetString *caller_gid)
{
	verbose("VideoClass: %s; SetVideoDecodeOffset", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: VideoClass_SetVideoDecodeOffset not yet implemented\n");
	return;
}

void
VideoClass_GetVideoDecodeOffset(VideoClass *t, GetVideoDecodeOffset *params, OctetString *caller_gid)
{
	verbose("VideoClass: %s; GetVideoDecodeOffset", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: VideoClass_GetVideoDecodeOffset not yet implemented\n");
	return;
}

void
VideoClass_ScaleVideo(VideoClass *t, ScaleVideo *params, OctetString *caller_gid)
{
	int x_scale;
	int y_scale;

	verbose("VideoClass: %s; ScaleVideo", ExternalReference_name(&t->rootClass.inst.ref));

	x_scale = GenericInteger_getInteger(&params->x_scale, caller_gid);
	y_scale = GenericInteger_getInteger(&params->y_scale, caller_gid);

/* TODO */
printf("TODO: VideoClass_ScaleVideo(%d, %d) not yet implemented\n", x_scale, y_scale);
	return;
}

void
VideoClass_render(VideoClass *t, MHEGDisplay *d, XYPosition *pos, OriginalBoxSize *box)
{
	XYPosition ins_pos;
	OriginalBoxSize ins_box;

	verbose("VideoClass: %s; render", ExternalReference_name(&t->rootClass.inst.ref));

	if(!intersects(pos, box, &t->inst.Position, &t->inst.BoxSize, &ins_pos, &ins_box))
		return;

	/* draw the video frame onto the Window contents Pixmap */
// need to do the equivalent of this, but not here
// => get rid of VideoClass.inst.player too
//	if(t->inst.player != NULL)
//		MHEGStreamPlayer_drawCurrentFrame(t->inst.player);

	/* make a transparent hole in the MHEG overlay so we can see the video below it */
	MHEGDisplay_fillTransparentRectangle(d, &ins_pos, &ins_box);

	return;
}

