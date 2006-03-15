/*
 * list.c
 */

/*
 * Copyright (C) 2005, Simon Kilvington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdio.h>
#include <limits.h>

#include "table.h"
#include "utils.h"

/* PID and TID we want */
#define PID_SDT		0x0011
#define TID_SDS		0x42

/* service_descriptor tag */
#define TAG_SERVICE_DESCRIPTOR	0x48

void
list_channels(unsigned int adapter, unsigned int timeout)
{
	char demux_dev[PATH_MAX];
	unsigned char *sds;
	uint16_t size;
	uint16_t offset;
	uint16_t service_id;
	uint16_t desc_loop_length;
	uint8_t desc_tag;
	uint8_t desc_length;
	uint8_t name_len;

	snprintf(demux_dev, sizeof(demux_dev), DEMUX_DEVICE, adapter);

	printf("Channels on this mutiplex:\n\n");
	printf("ID\tChannel\n");
	printf("==\t=======\n");

	/* grab the Service Description Section table */
	if((sds = read_table(demux_dev, PID_SDT, TID_SDS, timeout)) == NULL)
		fatal("Unable to read SDT");

	/* 12 bit section_length field */
	size = 3 + (((sds[1] & 0x0f) << 8) + sds[2]);

	/* loop through the services */
	offset = 11;
	/* -4 for the CRC at the end */
	while(offset < (size - 4))
	{
		service_id = (sds[offset] << 8) + sds[offset+1];
		printf("%u\t", service_id);
		/* move on to the descriptors */
		offset += 3;
		desc_loop_length = ((sds[offset] & 0x0f) << 8) + sds[offset+1];
		offset += 2;
		/* find the service_descriptor tag */
		while(desc_loop_length != 0)
		{
			desc_tag = sds[offset];
			desc_length = sds[offset+1];
			offset += 2;
			desc_loop_length -= 2;
			if(desc_tag == TAG_SERVICE_DESCRIPTOR)
			{
				/* service_type = sds[offset]; */
				offset += 1;
				/* service_provider_name */
				name_len = sds[offset];
				offset += 1 + name_len;
				/* service_name */
				name_len = sds[offset];
				offset += 1;
				printf("%.*s\n", name_len, &sds[offset]);
				offset += name_len;
			}
			else
			{
//				printf("desc_tag: %u\n", desc_tag);
//				hexdump(&sds[offset], desc_length);
				offset += desc_length;
			}
			desc_loop_length -= desc_length;
		}
	}

	return;
}

