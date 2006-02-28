/*
 * VariableClass.c
 */

#include "MHEGEngine.h"
#include "ISO13522-MHEG-5.h"
#include "RootClass.h"
#include "GenericBoolean.h"
#include "BooleanVariableClass.h"
#include "GenericInteger.h"
#include "IntegerVariableClass.h"
#include "GenericOctetString.h"
#include "OctetStringVariableClass.h"
#include "GenericObjectReference.h"
#include "ObjectRefVariableClass.h"
#include "GenericContentReference.h"
#include "ContentRefVariableClass.h"
#include "ExternalReference.h"
#include "ObjectReference.h"
#include "clone.h"

void
default_VariableClassInstanceVars(VariableClass *t, VariableClassInstanceVars *v)
{
	bzero(v, sizeof(VariableClassInstanceVars));

	/* take a copy of the OriginalValue */
	OriginalValue_dup(&v->Value, &t->original_value);

	return;
}

void
free_VariableClassInstanceVars(VariableClassInstanceVars *v)
{
	free_OriginalValue(&v->Value);

	return;
}

void
VariableClass_Preparation(VariableClass *t)
{
	verbose("VariableClass: %s; Preparation", ExternalReference_name(&t->rootClass.inst.ref));

	if(!RootClass_Preparation(&t->rootClass))
		return;

	default_VariableClassInstanceVars(t, &t->inst);

	return;
}

void
VariableClass_Activation(VariableClass *t)
{
	verbose("VariableClass: %s; Activation", ExternalReference_name(&t->rootClass.inst.ref));

	/* is it already activated */
	if(t->rootClass.inst.RunningStatus)
		return;

	/* has it been prepared yet */
	if(!t->rootClass.inst.AvailabilityStatus)
	{
		/* generates an IsAvailable event */
		VariableClass_Preparation(t);
	}

	t->rootClass.inst.RunningStatus = true;
	MHEGEngine_generateEvent(&t->rootClass.inst.ref, EventType_is_running, NULL);

	return;
}

void
VariableClass_Deactivation(VariableClass *t)
{
	verbose("VariableClass: %s; Deactivation", ExternalReference_name(&t->rootClass.inst.ref));

	RootClass_Deactivation(&t->rootClass);

	return;
}

void
VariableClass_Destruction(VariableClass *t)
{
	verbose("VariableClass: %s; Destruction", ExternalReference_name(&t->rootClass.inst.ref));

	if(!RootClass_Destruction(&t->rootClass))
		return;

	free_VariableClassInstanceVars(&t->inst);

	return;
}

void
VariableClass_SetData(VariableClass *t, SetData *set, OctetString *caller_gid)
{
	verbose("VariableClass: %s; SetData", ExternalReference_name(&t->rootClass.inst.ref));

/* TODO */
/* do we need the separate Boolean/Integer/etc/VariableClass_SetData funcs we have created? */
printf("TODO: VariableClass_SetData not yet implemented\n");
	return;
}

void
VariableClass_Clone(VariableClass *t, Clone *clone, OctetString *caller_gid)
{
	switch(t->inst.Value.choice)
	{
	case OriginalValue_boolean:
		BooleanVariableClass_Clone(t, clone, caller_gid);
		break;

	case OriginalValue_integer:
		IntegerVariableClass_Clone(t, clone, caller_gid);
		break;

	case OriginalValue_octetstring:
		OctetStringVariableClass_Clone(t, clone, caller_gid);
		break;

	case OriginalValue_object_reference:
		ObjectRefVariableClass_Clone(t, clone, caller_gid);
		break;

	case OriginalValue_content_reference:
		ContentRefVariableClass_Clone(t, clone, caller_gid);
		break;

	default:
		error("Unknown VariableClass %s; type: %d", ExternalReference_name(&t->rootClass.inst.ref), t->inst.Value.choice);
		break;
	}

	return;
}

/*
 * caller_gid is used to resolve the Generic variables in the new_value
 */

void
VariableClass_SetVariable(VariableClass *v, NewVariableValue *new_value, OctetString *caller_gid)
{
	switch(v->inst.Value.choice)
	{
	case OriginalValue_boolean:
		BooleanVariableClass_SetVariable(v, new_value, caller_gid);
		break;

	case OriginalValue_integer:
		IntegerVariableClass_SetVariable(v, new_value, caller_gid);
		break;

	case OriginalValue_octetstring:
		OctetStringVariableClass_SetVariable(v, new_value, caller_gid);
		break;

	case OriginalValue_object_reference:
		ObjectRefVariableClass_SetVariable(v, new_value, caller_gid);
		break;

	case OriginalValue_content_reference:
		ContentRefVariableClass_SetVariable(v, new_value, caller_gid);
		break;

	default:
		error("Unknown VariableClass %s; type: %d", ExternalReference_name(&v->rootClass.inst.ref), v->inst.Value.choice);
		break;
	}

	return;
}

/*
 * caller_gid is used to resolve the Generic variables in the ComparisionValue
 */

void
VariableClass_TestVariable(VariableClass *v, int op, ComparisonValue *comp, OctetString *caller_gid)
{
	/* check the types match */
	if(v->inst.Value.choice == OriginalValue_boolean
	&& comp->choice == ComparisonValue_new_generic_boolean)
	{
		bool val = GenericBoolean_getBoolean(&comp->u.new_generic_boolean, caller_gid);
		BooleanVariableClass_TestVariable(v, op, val);
	}
	else if(v->inst.Value.choice == OriginalValue_integer
	&& comp->choice == ComparisonValue_new_generic_integer)
	{
		int val = GenericInteger_getInteger(&comp->u.new_generic_integer, caller_gid);
		IntegerVariableClass_TestVariable(v, op, val);
	}
	else if(v->inst.Value.choice == OriginalValue_octetstring
	&& comp->choice == ComparisonValue_new_generic_octetstring)
	{
		OctetString *val = GenericOctetString_getOctetString(&comp->u.new_generic_octetstring, caller_gid);
		OctetStringVariableClass_TestVariable(v, op, val);
	}
	else if(v->inst.Value.choice == OriginalValue_object_reference
	&& comp->choice == ComparisonValue_new_generic_object_reference)
	{
		ObjectReference *val = GenericObjectReference_getObjectReference(&comp->u.new_generic_object_reference, caller_gid);
		/* need to pass the caller_gid as the ObjectReference may not contain a group id */
		ObjectRefVariableClass_TestVariable(v, op, val, caller_gid);
	}
	else if(v->inst.Value.choice == OriginalValue_content_reference
	&& comp->choice == ComparisonValue_new_generic_content_reference)
	{
		ContentReference *val = GenericContentReference_getContentReference(&comp->u.new_generic_content_reference, caller_gid);
		ContentRefVariableClass_TestVariable(v, op, val);
	}
	else
	{
		error("TestVariable: %s; type mismatch", ExternalReference_name(&v->rootClass.inst.ref));
	}

	return;
}

/*
 * returns OriginalValue_boolean/integer/octetstring/content_reference/object_reference
 */

unsigned int
VariableClass_type(VariableClass *v)
{
	return v->inst.Value.choice;
}

