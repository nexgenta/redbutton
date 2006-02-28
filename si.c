/*
 * si.c
 */

#include <ctype.h>

#include "si.h"
#include "MHEGEngine.h"
#include "utils.h"

/* looks like we can just make this index up */
static int si_max_index = -1;
static OctetString *si_channel = NULL;

int
si_get_index(OctetString *ref)
{
	int i;

	/* have we assigned it already */
	for(i=0; i<=si_max_index; i++)
		if(OctetString_cmp(ref, &si_channel[i]) == 0)
			return i;

	/* add it to the list */
	si_max_index ++;
	si_channel = safe_realloc(si_channel, (si_max_index + 1) * sizeof(OctetString));
	OctetString_dup(&si_channel[si_max_index], ref);

	return si_max_index;
}

bool
si_tune_index(int index)
{
	if(index > si_max_index)
	{
		error("SI_TuneIndex: invalid service index (%d); max is %d", index, si_max_index);
		return false;
	}

	MHEGEngine_quit(QuitReason_Retune, &si_channel[index]);

	return true;
}

unsigned int
si_get_service_id(OctetString *ref)
{
	unsigned int len;
	unsigned int id;

	len = ref->size;
	while(len > 0 && isxdigit(ref->data[len - 1]))
		len --;

	id = 0;
	while(len < ref->size)
	{
		id <<= 4;
		id += char2hex(ref->data[len]);
		len ++;
	}

	return id;
}

void
si_free(void)
{
	int i;

	for(i=0; i<=si_max_index; i++)
		safe_free(si_channel[i].data);

	safe_free(si_channel);

	return;
}

