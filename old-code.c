print values of variables in ElementaryAction_execute

{
VariableClass *var;
ObjectReference ref;
ref.choice = ObjectReference_external_reference;
ref.u.external_reference.object_number = 204;
ref.u.external_reference.group_identifier.size = 3;
ref.u.external_reference.group_identifier.data = "~/a";
if((var = (VariableClass *) MHEGEngine_findObjectReference(&ref, NULL)) && var->rootClass.inst.rtti == RTTI_VariableClass)
{
	printf("%s = ", ObjectReference_name(&ref));
	printf("%s\n", ObjectReference_name(&var->inst.Value->u.object_reference));
}
ref.u.external_reference.object_number = 234;
if((var = (VariableClass *) MHEGEngine_findObjectReference(&ref, NULL)) && var->rootClass.inst.rtti == RTTI_VariableClass)
{
	printf("%s = ", ObjectReference_name(&ref));
	printf("%.*s\n", var->inst.Value->u.octetstring.size, var->inst.Value->u.octetstring.data);
}
}

---------------------------------------------------------------------------------------------

print active links in MHEGEngine_run

{
LIST_TYPE(LinkClassPtr) *l = engine.active_links;
while(l)
{
	printf("%s; ", ObjectReference_name(&l->item->link_condition.event_source));
	printf("%s ", EventType_name(l->item->link_condition.event_type));
	if(l->item->link_condition.have_event_data && l->item->link_condition.event_data.choice == EventData_integer)
		printf("%d", l->item->link_condition.event_data.u.integer);
	printf("\n");
	l = l->next;
}
printf("\n");
}

---------------------------------------------------------------------------------------------

from MHEGDisplay_newBitmap()

this works, ie creates a scaled up copy of the bitmap and mask
but I can't get rid of the black line along the bottom and right hand side

/* set the xform matrix */
...
/* set the filters */
...
/* cache the scaled image */
Pixmap unscaled;
Picture unscaled_pic;
XRenderColor transparent = { 0, 0, 0, 0 };
XRenderColor opaque = { 65535, 65535, 65535, 65535 };
unsigned int scaled_width = (c->width * d->xres) / MHEG_XRES;
unsigned int scaled_height = (c->height * d->yres) / MHEG_YRES;

unscaled = b->image;
unscaled_pic = b->image_pic;
b->image = XCreatePixmap(d->dpy, d->win, scaled_width, scaled_height, d->depth);
b->image_pic = XRenderCreatePicture(d->dpy, b->image, d->pic_format, 0, NULL);
XRenderFillRectangle(d->dpy, PictOpSrc, b->image_pic, &transparent, 0, 0, scaled_width, scaled_height);
XRenderComposite(d->dpy, PictOpOver, unscaled_pic, None, b->image_pic, 0, 0, 0, 0, 0, 0, scaled_width, scaled_height);
XRenderFreePicture(d->dpy, unscaled_pic);
XFreePixmap(d->dpy, unscaled);
/* cache the scaled mask */
unscaled = b->mask;
unscaled_pic = b->mask_pic;
b->mask = XCreatePixmap(d->dpy, d->win, scaled_width, scaled_height, d->depth);
b->mask_pic = XRenderCreatePicture(d->dpy, b->mask, d->pic_format, 0, NULL);
XRenderFillRectangle(d->dpy, PictOpSrc, b->mask_pic, &opaque, 0, 0, scaled_width, scaled_height);
XRenderComposite(d->dpy, PictOpOver, unscaled_pic, None, b->mask_pic, 0, 0, 0, 0, 0, 0, scaled_width, scaled_height);
XRenderFreePicture(d->dpy, unscaled_pic);
XFreePixmap(d->dpy, unscaled);


-----------------------------

UK MHEG Profile doesnt need to support ButtonClass or its derivatives

<HotspotClass>
typedef struct HotspotClassInstanceVars
{
	/* inherited from VisibleClass */
	OriginalBoxSize *BoxSize;
	XYPosition *Position;
	bool have_PaletteRef;		/* OPTIONAL */
	ObjectReference *PaletteRef;
	/* inherited from ButtonClass */
	bool SelectionStatus;
} HotspotClassInstanceVars;
</HotspotClass>

void
default_HotspotClassInstanceVars(HotspotClass *t, HotspotClassInstanceVars *v)
{
	bzero(v, sizeof(HotspotClassInstanceVars));

	/* VisibleClass */
	v->BoxSize = &t->ButtonClass.original_box_size;
	v->Position = &t->ButtonClass.original_position;
	v->have_PaletteRef = t->ButtonClass.have_original_palette_ref;
	v->PaletteRef= &t->ButtonClass.original_palette_ref;

	/* ButtonClass */
	v->SelectionStatus = false;

	return;
}

void
free_HotspotClassInstanceVars(HotspotClassInstanceVars *v)
{
	return;
}

<PushButtonClass>
typedef struct PushButtonClassInstanceVars
{
	/* inherited from VisibleClass */
	OriginalBoxSize *BoxSize;
	XYPosition *Position;
	bool have_PaletteRef;		/* OPTIONAL */
	ObjectReference *PaletteRef;
	/* inherited from ButtonClass */
	bool SelectionStatus;
	/* PushButtonClass */
	bool have_Label;	/* OPTIONAL */
	OctetString *Label;
} PushButtonClassInstanceVars;
</PushButtonClass>

void
default_PushButtonClassInstanceVars(PushButtonClass *t, PushButtonClassInstanceVars *v)
{
	bzero(v, sizeof(PushButtonClassInstanceVars));

	/* VisibleClass */
	v->BoxSize = &t->original_box_size;
	v->Position = &t->original_position;
	v->have_PaletteRef = t->have_original_palette_ref;
	v->PaletteRef= &t->original_palette_ref;

	/* ButtonClass */
	v->SelectionStatus = false;

	/* PushButtonClass */
	v->have_Label = t->have_original_label;
	v->Label = &t->original_label;

	return;
}

void
free_PushButtonClassInstanceVars(PushButtonClassInstanceVars *v)
{
	return;
}

<SwitchButtonClass>
typedef struct SwitchButtonClassInstanceVars
{
	/* inherited from VisibleClass */
	OriginalBoxSize *BoxSize;
	XYPosition *Position;
	bool have_PaletteRef;		/* OPTIONAL */
	ObjectReference *PaletteRef;
	/* inherited from ButtonClass */
	bool SelectionStatus;
	/* inherited from PushButtonClass */
	bool have_Label;	/* OPTIONAL */
	OctetString *Label;
} SwitchButtonClassInstanceVars;
</SwitchButtonClass>

void
default_SwitchButtonClassInstanceVars(SwitchButtonClass *t, SwitchButtonClassInstanceVars *v)
{
	bzero(v, sizeof(SwitchButtonClassInstanceVars));

	/* VisibleClass */
	v->BoxSize = &t->original_box_size;
	v->Position = &t->original_position;
	v->have_PaletteRef = t->have_original_palette_ref;
	v->PaletteRef= &t->original_palette_ref;

	/* ButtonClass */
	v->SelectionStatus = false;

	/* PushButtonClass */
	v->have_Label = t->have_original_label;
	v->Label = &t->original_label;

	return;
}

void
free_SwitchButtonClassInstanceVars(SwitchButtonClassInstanceVars *v)
{
	return;
}



-----------------------------------------------------------------------------------------------------------

UK MHEG Profile doesnt need to support RTGraphicsClass

<RTGraphicsClass>
typedef struct RTGraphicsClassInstanceVars
{
	/* inherited from VisibleClass */
	OriginalBoxSize *BoxSize;
	XYPosition *Position;
	bool have_PaletteRef;		/* OPTIONAL */
	ObjectReference *PaletteRef;
} RTGraphicsClassInstanceVars;
</RTGraphicsClass>

void
default_RTGraphicsClassInstanceVars(RTGraphicsClass *t, RTGraphicsClassInstanceVars *v)
{
	bzero(v, sizeof(RTGraphicsClassInstanceVars));

	/* VisibleClass */
	v->BoxSize = &t->original_box_size;
	v->Position = &t->original_position;
	v->have_PaletteRef = t->have_original_palette_ref;
	v->PaletteRef= &t->original_palette_ref;

	return;
}

void
free_RTGraphicsClassInstanceVars(RTGraphicsClassInstanceVars *v)
{
	return;
}

-----------------------------------------------------------------------------------------------------

VisibleClass and IngredientClass are abstract and so should never be created

<VisibleClass>
typedef struct VisibleClassInstanceVars
{
	OriginalBoxSize *BoxSize;
	XYPosition *Position;
	bool have_PaletteRef;		/* OPTIONAL */
	ObjectReference *PaletteRef;
} VisibleClassInstanceVars;
</VisibleClass>

void
default_VisibleClassInstanceVars(VisibleClass *t, VisibleClassInstanceVars *v)
{
	bzero(v, sizeof(VisibleClassInstanceVars));

	v->BoxSize = &t->original_box_size;
	v->Position = &t->original_position;
	v->have_PaletteRef = t->have_original_palette_ref;
	v->PaletteRef= &t->original_palette_ref;

	return;
}

void
free_VisibleClassInstanceVars(VisibleClassInstanceVars *v)
{
	return;
}

<IngredientClass>
typedef struct IngredientClassInstanceVars
{
	bool have_Content;	/* OPTIONAL */
	ContentBody *Content;
} IngredientClassInstanceVars;
</IngredientClass>

void
default_IngredientClassInstanceVars(IngredientClass *t, IngredientClassInstanceVars *v)
{
	bzero(v, sizeof(IngredientClassInstanceVars));

	v->have_Content = t->have_original_content;
	v->Content = &t->original_content;

	return;
}

void
free_IngredientClassInstanceVars(IngredientClassInstanceVars *v)
{
	return;
}

-------------------------------------------------------------------------------------------------



/*
 * IngredientClass.c
 */

#include "MHEGEngine.h"
#include "IngredientClass.h"
#include "RemoteProgramClass.h"
#include "ResidentProgramClass.h"
#include "InterchangedProgramClass.h"
#include "PaletteClass.h"
#include "FontClass.h"
#include "CursorShapeClass.h"
#include "VariableClass.h"
#include "LinkClass.h"
#include "AudioClass.h"
#include "VideoClass.h"
#include "RTGraphicsClass.h"
#include "StreamClass.h"
#include "BitmapClass.h"
#include "DynamicLineArtClass.h"
#include "RectangleClass.h"
#include "HotspotClass.h"
#include "SwitchButtonClass.h"
#include "PushButtonClass.h"
#include "TextClass.h"
#include "EntryFieldClass.h"
#include "HyperTextClass.h"
#include "SliderClass.h"
#include "TokenGroupClass.h"
#include "ListGroupClass.h"
#include "rtti.h"

void
IngredientClass_Unload(RootClass *r)
{
	switch(r->inst.rtti)
	{
	case RTTI_RemoteProgramClass:
		RemoteProgramClass_Destruction((RemoteProgramClass *) r);
		break;

	case RTTI_ResidentProgramClass:
		ResidentProgramClass_Destruction((ResidentProgramClass *) r);
		break;

	case RTTI_InterchangedProgramClass:
		InterchangedProgramClass_Destruction((InterchangedProgramClass *) r);
		break;

	case RTTI_PaletteClass:
		PaletteClass_Destruction((PaletteClass *) r);
		break;

	case RTTI_FontClass:
		FontClass_Destruction((FontClass *) r);
		break;

	case RTTI_CursorShapeClass:
		CursorShapeClass_Destruction((CursorShapeClass *) r);
		break;

	case RTTI_VariableClass:
		VariableClass_Destruction((VariableClass *) r);
		break;

	case RTTI_LinkClass:
		LinkClass_Destruction((LinkClass *) r);
		break;

	case RTTI_AudioClass:
		AudioClass_Destruction((AudioClass *) r);
		break;

	case RTTI_VideoClass:
		VideoClass_Destruction((VideoClass *) r);
		break;

	case RTTI_RTGraphicsClass:
		RTGraphicsClass_Destruction((RTGraphicsClass *) r);
		break;

	case RTTI_StreamClass:
		StreamClass_Destruction((StreamClass *) r);
		break;

	case RTTI_BitmapClass:
		BitmapClass_Destruction((BitmapClass *) r);
		break;

	case RTTI_DynamicLineArtClass:
		DynamicLineArtClass_Destruction((DynamicLineArtClass *) r);
		break;

	case RTTI_RectangleClass:
		RectangleClass_Destruction((RectangleClass *) r);
		break;

	case RTTI_HotspotClass:
		HotspotClass_Destruction((HotspotClass *) r);
		break;

	case RTTI_SwitchButtonClass:
		SwitchButtonClass_Destruction((SwitchButtonClass *) r);
		break;

	case RTTI_PushButtonClass:
		PushButtonClass_Destruction((PushButtonClass *) r);
		break;

	case RTTI_TextClass:
		TextClass_Destruction((TextClass *) r);
		break;

	case RTTI_EntryFieldClass:
		EntryFieldClass_Destruction((EntryFieldClass *) r);
		break;

	case RTTI_HyperTextClass:
		HyperTextClass_Destruction((HyperTextClass *) r);
		break;

	case RTTI_SliderClass:
		SliderClass_Destruction((SliderClass *) r);
		break;

	case RTTI_TokenGroupClass:
		TokenGroupClass_Destruction((TokenGroupClass *) r);
		break;

	case RTTI_ListGroupClass:
		ListGroupClass_Destruction((ListGroupClass *) r);
		break;

	default:
		error("Unknown IngredientClass type: %d", r->inst.rtti);
		break;
	}

	return;
}

-------------------------------------------------------------------------------------

hand written dup/copy routines...

/*
 * calls safe_free on any existing data in dst
 * (use ObjectReference_dup if dst is not initialised)
 */

void
ObjectReference_copy(ObjectReference *dst, ObjectReference *src)
{
	/* free any existing data */
	if(dst->choice == ObjectReference_external_reference)
		safe_free(dst->u.external_reference.group_identifier.data);

	dst->choice = src->choice;

	switch(src->choice)
	{
	case ObjectReference_internal_reference:
		dst->u.internal_reference = src->u.internal_reference;
		break;

	case ObjectReference_external_reference:
		OctetString_dup(&dst->u.external_reference.group_identifier, &src->u.external_reference.group_identifier);
		dst->u.external_reference.object_number = src->u.external_reference.object_number;
		break;

	default:
		error("Unknown ObjectReference type: %d", src->choice);
		break;
	}

	return;
}

/*
 * should only be used when dst is uninitialised
 * any existing data in dst will be lost
 * (use ObjectReference_copy if dst is initialised)
 */

void
ObjectReference_dup(ObjectReference *dst, ObjectReference *src)
{
	dst->choice = src->choice;

	switch(src->choice)
	{
	case ObjectReference_internal_reference:
		dst->u.internal_reference = src->u.internal_reference;
		break;

	case ObjectReference_external_reference:
		OctetString_dup(&dst->u.external_reference.group_identifier, &src->u.external_reference.group_identifier);
		dst->u.external_reference.object_number = src->u.external_reference.object_number;
		break;

	default:
		error("Unknown ObjectReference type: %d", src->choice);
		break;
	}

	return;
}

/*
 * free's any exisiting data
 */

void
OriginalValue_copy(OriginalValue *dst, OriginalValue *src)
{
	/* free any old data */
	free_OriginalValue(dst);

	/* dup the new data */
	OriginalValue_dup(dst, src);

	return;
}

/*
 * any existing data in dst will be lost
 */

void
OriginalValue_dup(OriginalValue *dst, OriginalValue *src)
{
	dst->choice = src->choice;

	switch(dst->choice)
	{
	case OriginalValue_boolean:
		dst->u.boolean = src->u.boolean;
		break;

	case OriginalValue_integer:
		dst->u.integer = src->u.integer;
		break;

	case OriginalValue_octetstring:
		OctetString_dup(&dst->u.octetstring, &src->u.octetstring);
		break;

	case OriginalValue_object_reference:
		ObjectReference_dup(&dst->u.object_reference, &src->u.object_reference);
		break;

	case OriginalValue_content_reference:
		/* ContentReference is just an OctetString */
		OctetString_dup(&dst->u.content_reference, &src->u.content_reference);
		break;

	default:
		error("Unknown OriginalValue type: %d", dst->choice);
		break;
	}

	return;
}

