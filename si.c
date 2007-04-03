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

/* TODO */
/* convert rec://svc/{def,cur,lcn/X} to dvb://network.transport.service */
/* even if we don't have a backend! */
if(ref->size < 6 || strncmp(ref->data, "dvb://", 6) != 0)
printf("TODO: si_get_index '%.*s' is not in 'dvb://' format\n", ref->size, ref->data);

	/* add it to the list */
	si_max_index ++;
	si_channel = safe_realloc(si_channel, (si_max_index + 1) * sizeof(OctetString));
	OctetString_dup(&si_channel[si_max_index], ref);

	return si_max_index;
}

OctetString *
si_get_url(int index)
{
	if(index > si_max_index)
	{
		error("SI_GetURL: invalid service index (%d); max is %d", index, si_max_index);
		return NULL;
	}

	return &si_channel[index];
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

/*
 * URL format is:
 * dvb://original_network_id.[transport_id].service_id
 * each id is a hex value without any preceeding 0x etc
 */

unsigned int
si_get_network_id(OctetString *ref)
{
	unsigned int pos;
	unsigned int id;

	if(ref == NULL || ref->size < 6 || strncmp(ref->data, "dvb://", 6) != 0)
		return 0;

	/* read upto the first . or end of string */
	id = 0;
	pos = 6;
	while(pos < ref->size && isxdigit(ref->data[pos]))
	{
		id <<= 4;
		id += char2hex(ref->data[pos]);
		pos ++;
	}

	return id;
}

unsigned int
si_get_transport_id(OctetString *ref)
{
	unsigned int pos;
	unsigned int id;

	if(ref == NULL || ref->size < 6 || strncmp(ref->data, "dvb://", 6) != 0)
		return 0;

	/* find the first . or end of string */
	pos = 6;
	while(pos < ref->size && ref->data[pos] != '.')
		pos ++;

	/* skip the . */
	pos ++;

	/* read the value */
	id = 0;
	while(pos < ref->size && isxdigit(ref->data[pos]))
	{
		id <<= 4;
		id += char2hex(ref->data[pos]);
		pos ++;
	}

	return id;
}

unsigned int
si_get_service_id(OctetString *ref)
{
	unsigned int len;
	unsigned int id;

	if(ref == NULL || ref->size < 6 || strncmp(ref->data, "dvb://", 6) != 0)
		return 0;

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

