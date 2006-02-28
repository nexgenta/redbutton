/*
 * ActionClass.c
 */

#include "MHEGEngine.h"
#include "ActionClass.h"
#include "ElementaryAction.h"

/*
 * caller_gid should be the group identifier of the object containing the ActionClass
 * it is used to resolve the ObjectReference's in the ElementaryAction's
 */

void
ActionClass_execute(ActionClass *a, OctetString *caller_gid)
{
	LIST_TYPE(ElementaryAction) *list = *a;

	while(list)
	{
		ElementaryAction_execute(&list->item, caller_gid);
		list = list->next;
	}

	return;
}

