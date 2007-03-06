/*
 * DynamicLineArtClass.c
 */

#include "MHEGEngine.h"
#include "DynamicLineArtClass.h"
#include "LineArtClass.h"
#include "RootClass.h"
#include "ExternalReference.h"
#include "GenericObjectReference.h"
#include "GenericInteger.h"
#include "VariableClass.h"
#include "IntegerVariableClass.h"
#include "rtti.h"

void
DynamicLineArtClass_Preparation(DynamicLineArtClass *t)
{
	verbose("DynamicLineArtClass: %s; Preparation", ExternalReference_name(&t->rootClass.inst.ref));

	/* RootClass Preparation */
	if(!RootClass_Preparation(&t->rootClass))
		return;

	default_LineArtClassInstanceVars(t, &t->inst);

	/* add it to the DisplayStack of the active application */
	MHEGEngine_addVisibleObject(&t->rootClass);

	return;
}

void
DynamicLineArtClass_Activation(DynamicLineArtClass *t)
{
	verbose("DynamicLineArtClass: %s; Activation", ExternalReference_name(&t->rootClass.inst.ref));

	/* has it been prepared yet */
	if(!t->rootClass.inst.AvailabilityStatus)
		DynamicLineArtClass_Preparation(t);

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
DynamicLineArtClass_Deactivation(DynamicLineArtClass *t)
{
	verbose("DynamicLineArtClass: %s; Deactivation", ExternalReference_name(&t->rootClass.inst.ref));

	/* is it already deactivated */
	if(!RootClass_Deactivation(&t->rootClass))
		return;

	/* now its RunningStatus is false, redraw the area it covered */
	MHEGEngine_redrawArea(&t->inst.Position, &t->inst.BoxSize);

	return;
}

void
DynamicLineArtClass_Destruction(DynamicLineArtClass *t)
{
	verbose("DynamicLineArtClass: %s; Destruction", ExternalReference_name(&t->rootClass.inst.ref));

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
		DynamicLineArtClass_Deactivation(t);
	}

/* TODO caching */
	free_LineArtClassInstanceVars(&t->inst);

	/* generate an IsDeleted event */
	t->rootClass.inst.AvailabilityStatus = false;
	MHEGEngine_generateEvent(&t->rootClass.inst.ref, EventType_is_deleted, NULL);

	return;
}

void
DynamicLineArtClass_SetData(DynamicLineArtClass *t, SetData *set, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; SetData", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_SetData not yet implemented\n");
	return;
}

void
DynamicLineArtClass_Clone(DynamicLineArtClass *t, Clone *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; Clone", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_Clone not yet implemented\n");
	return;
}

void
DynamicLineArtClass_SetPosition(DynamicLineArtClass *t, SetPosition *params, OctetString *caller_gid)
{
	XYPosition old;

	verbose("DynamicLineArtClass: %s; SetPosition", ExternalReference_name(&t->rootClass.inst.ref));

	/* corrigendum says we don't need to clear to OriginalRefFillColour */

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
DynamicLineArtClass_GetPosition(DynamicLineArtClass *t, GetPosition *params, OctetString *caller_gid)
{
	VariableClass *var;

	verbose("DynamicLineArtClass: %s; GetPosition", ExternalReference_name(&t->rootClass.inst.ref));

	/* X position */
	if((var = (VariableClass *) MHEGEngine_findObjectReference(&params->x_position_var, caller_gid)) == NULL)
		return;

	if(var->rootClass.inst.rtti != RTTI_VariableClass
	|| VariableClass_type(var) != OriginalValue_integer)
	{
		error("DynamicLineArtClass: GetPosition: type mismatch");
		return;
	}

	IntegerVariableClass_setInteger(var, t->inst.Position.x_position);

	/* Y position */
	if((var = (VariableClass *) MHEGEngine_findObjectReference(&params->y_position_var, caller_gid)) == NULL)
		return;

	if(var->rootClass.inst.rtti != RTTI_VariableClass
	|| VariableClass_type(var) != OriginalValue_integer)
	{
		error("DynamicLineArtClass: GetPosition: type mismatch");
		return;
	}

	IntegerVariableClass_setInteger(var, t->inst.Position.y_position);

	return;
}

void
DynamicLineArtClass_SetBoxSize(DynamicLineArtClass *t, SetBoxSize *params, OctetString *caller_gid)
{
	OriginalBoxSize old;

	verbose("DynamicLineArtClass: %s; SetBoxSize", ExternalReference_name(&t->rootClass.inst.ref));

	old.x_length = t->inst.BoxSize.x_length;
	old.y_length = t->inst.BoxSize.y_length;

	t->inst.BoxSize.x_length = GenericInteger_getInteger(&params->x_new_box_size, caller_gid);
	t->inst.BoxSize.y_length = GenericInteger_getInteger(&params->y_new_box_size, caller_gid);

/* TODO */
/* clear to OriginalRefFillColour */
printf("TODO: DynamicLineArtClass_SetBoxSize clear to OriginalRefFillColour\n");

	/* if it is active, redraw it */
	if(t->rootClass.inst.RunningStatus)
	{
		MHEGEngine_redrawArea(&t->inst.Position, &old);
		MHEGEngine_redrawArea(&t->inst.Position, &t->inst.BoxSize);
	}

	return;
}

void
DynamicLineArtClass_GetBoxSize(DynamicLineArtClass *t, GetBoxSize *params, OctetString *caller_gid)
{
	VariableClass *var;

	verbose("DynamicLineArtClass: %s; GetBoxSize", ExternalReference_name(&t->rootClass.inst.ref));

	/* width */
	if((var = (VariableClass *) MHEGEngine_findObjectReference(&params->x_box_size_var, caller_gid)) == NULL)
		return;

	if(var->rootClass.inst.rtti != RTTI_VariableClass
	|| VariableClass_type(var) != OriginalValue_integer)
	{
		error("DynamicLineArtClass: GetBoxSize: type mismatch");
		return;
	}

	IntegerVariableClass_setInteger(var, t->inst.BoxSize.x_length);

	/* height */
	if((var = (VariableClass *) MHEGEngine_findObjectReference(&params->y_box_size_var, caller_gid)) == NULL)
		return;

	if(var->rootClass.inst.rtti != RTTI_VariableClass
	|| VariableClass_type(var) != OriginalValue_integer)
	{
		error("DynamicLineArtClass: GetBoxSize: type mismatch");
		return;
	}

	IntegerVariableClass_setInteger(var, t->inst.BoxSize.y_length);

	return;
}

void
DynamicLineArtClass_BringToFront(DynamicLineArtClass *t)
{
	verbose("DynamicLineArtClass: %s; BringToFront", ExternalReference_name(&t->rootClass.inst.ref));

	MHEGEngine_bringToFront(&t->rootClass);

	/* corrigendum says we don't need to clear to OriginalRefFillColour */

	/* if it is active, redraw it */
	if(t->rootClass.inst.RunningStatus)
		MHEGEngine_redrawArea(&t->inst.Position, &t->inst.BoxSize);

	return;
}

void
DynamicLineArtClass_SendToBack(DynamicLineArtClass *t)
{
	verbose("DynamicLineArtClass: %s; SendToBack", ExternalReference_name(&t->rootClass.inst.ref));

	MHEGEngine_sendToBack(&t->rootClass);

	/* corrigendum says we don't need to clear to OriginalRefFillColour */

	/* if it is active, redraw it */
	if(t->rootClass.inst.RunningStatus)
		MHEGEngine_redrawArea(&t->inst.Position, &t->inst.BoxSize);

	return;
}

void
DynamicLineArtClass_PutBefore(DynamicLineArtClass *t, PutBefore *params, OctetString *caller_gid)
{
	ObjectReference *ref;
	RootClass *obj;

	verbose("DynamicLineArtClass: %s; PutBefore", ExternalReference_name(&t->rootClass.inst.ref));

	if(((ref = GenericObjectReference_getObjectReference(&params->reference_visible, caller_gid)) != NULL)
	&& ((obj = MHEGEngine_findObjectReference(ref, caller_gid)) != NULL))
	{
		MHEGEngine_putBefore(&t->rootClass, obj);
		/* corrigendum says we don't need to clear to OriginalRefFillColour */
		/* if it is active, redraw it */
		if(t->rootClass.inst.RunningStatus)
			MHEGEngine_redrawArea(&t->inst.Position, &t->inst.BoxSize);
	}

	return;
}

void
DynamicLineArtClass_PutBehind(DynamicLineArtClass *t, PutBehind *params, OctetString *caller_gid)
{
	ObjectReference *ref;
	RootClass *obj;

	verbose("DynamicLineArtClass: %s; PutBehind", ExternalReference_name(&t->rootClass.inst.ref));

	if(((ref = GenericObjectReference_getObjectReference(&params->reference_visible, caller_gid)) != NULL)
	&& ((obj = MHEGEngine_findObjectReference(ref, caller_gid)) != NULL))
	{
		MHEGEngine_putBehind(&t->rootClass, obj);
		/* corrigendum says we don't need to clear to OriginalRefFillColour */
		/* if it is active, redraw it */
		if(t->rootClass.inst.RunningStatus)
			MHEGEngine_redrawArea(&t->inst.Position, &t->inst.BoxSize);
	}

	return;
}

void
DynamicLineArtClass_SetPaletteRef(DynamicLineArtClass *t, SetPaletteRef *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; SetPaletteRef", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_SetPaletteRef not yet implemented\n");
	return;
}

void
DynamicLineArtClass_SetLineWidth(DynamicLineArtClass *t, SetLineWidth *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; SetLineWidth", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_SetLineWidth not yet implemented\n");
	return;
}

void
DynamicLineArtClass_SetLineStyle(DynamicLineArtClass *t, SetLineStyle *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; SetLineStyle", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_SetLineStyle not yet implemented\n");
	return;
}

void
DynamicLineArtClass_SetLineColour(DynamicLineArtClass *t, SetLineColour *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; SetLineColour", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_SetLineColour not yet implemented\n");
	return;
}

void
DynamicLineArtClass_SetFillColour(DynamicLineArtClass *t, SetFillColour *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; SetFillColour", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_SetFillColour not yet implemented\n");
	return;
}

void
DynamicLineArtClass_GetLineWidth(DynamicLineArtClass *t, GetLineWidth *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; GetLineWidth", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_GetLineWidth not yet implemented\n");
	return;
}

void
DynamicLineArtClass_GetLineStyle(DynamicLineArtClass *t, GetLineStyle *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; GetLineStyle", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_GetLineStyle not yet implemented\n");
	return;
}

void
DynamicLineArtClass_GetLineColour(DynamicLineArtClass *t, GetLineColour *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; GetLineColour", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_GetLineColour not yet implemented\n");
	return;
}

void
DynamicLineArtClass_GetFillColour(DynamicLineArtClass *t, GetFillColour *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; GetFillColour", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_GetFillColour not yet implemented\n");
	return;
}

void
DynamicLineArtClass_DrawArc(DynamicLineArtClass *t, DrawArc *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; DrawArc", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_DrawArc not yet implemented\n");
	return;
}

void
DynamicLineArtClass_DrawSector(DynamicLineArtClass *t, DrawSector *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; DrawSector", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_DrawSector not yet implemented\n");
	return;
}

void
DynamicLineArtClass_DrawLine(DynamicLineArtClass *t, DrawLine *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; DrawLine", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_DrawLine not yet implemented\n");
	return;
}

void
DynamicLineArtClass_DrawOval(DynamicLineArtClass *t, DrawOval *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; DrawOval", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_DrawOval not yet implemented\n");
	return;
}

void
DynamicLineArtClass_DrawPolygon(DynamicLineArtClass *t, DrawPolygon *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; DrawPolygon", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_DrawPolygon not yet implemented\n");
	return;
}

void
DynamicLineArtClass_DrawPolyline(DynamicLineArtClass *t, DrawPolyline *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; DrawPolyline", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_DrawPolyline not yet implemented\n");
	return;
}

void
DynamicLineArtClass_DrawRectangle(DynamicLineArtClass *t, DrawRectangle *params, OctetString *caller_gid)
{
	verbose("DynamicLineArtClass: %s; DrawRectangle", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_DrawRectangle not yet implemented\n");
	return;
}

void
DynamicLineArtClass_Clear(DynamicLineArtClass *t)
{
	verbose("DynamicLineArtClass: %s; Clear", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
printf("TODO: DynamicLineArtClass_Clear not yet implemented\n");
	return;
}

void
DynamicLineArtClass_render(DynamicLineArtClass *t, MHEGDisplay *d, XYPosition *pos, OriginalBoxSize *box)
{
	XYPosition ins_pos;
	OriginalBoxSize ins_box;

	verbose("DynamicLineArtClass: %s; render", ExternalReference_name(&t->rootClass.inst.ref));

	if(!intersects(pos, box, &t->inst.Position, &t->inst.BoxSize, &ins_pos, &ins_box))
		return;

	MHEGDisplay_setClipRectangle(d, &ins_pos, &ins_box);

/* TODO */
printf("TODO: DynamicLineArtClass_render\n");

	MHEGDisplay_unsetClipRectangle(d);

	return;
}

