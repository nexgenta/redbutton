/*
 * SwitchButtonClass.c
 */

#include "MHEGEngine.h"
#include "SwitchButtonClass.h"
#include "ExternalReference.h"

void
SwitchButtonClass_Preparation(SwitchButtonClass *t)
{
	error("SwitchButtonClass: %s; not supported", ExternalReference_name(&t->rootClass.inst.ref));

	return;
}

void
SwitchButtonClass_Activation(SwitchButtonClass *t)
{
	error("SwitchButtonClass: %s; not supported", ExternalReference_name(&t->rootClass.inst.ref));

	return;
}

void
SwitchButtonClass_Deactivation(SwitchButtonClass *t)
{
	error("SwitchButtonClass: %s; not supported", ExternalReference_name(&t->rootClass.inst.ref));

	return;
}

void
SwitchButtonClass_Destruction(SwitchButtonClass *t)
{
	error("SwitchButtonClass: %s; not supported", ExternalReference_name(&t->rootClass.inst.ref));

	return;
}

