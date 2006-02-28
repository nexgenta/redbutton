/*
 * findmheg.c
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
#include <stdint.h>
#include <netinet/in.h>

#include "carousel.h"
#include "table.h"
#include "assoc.h"
#include "utils.h"

/* Programme Association Table and Section */
#define PID_PAT		0x0000
#define TID_PAT		0x00

/* Programme Map Section */
#define TID_PMT		0x02

/* stream_type we are interested in */
#define STREAM_TYPE_ISO13818_6_B	0x0b

/* descriptors we want */
#define TAG_CAROUSEL_ID_DESCRIPTOR	0x13
#define TAG_STREAM_ID_DESCRIPTOR	0x52

/* bits of the descriptors we care about */
struct carousel_id_descriptor
{
	uint32_t carousel_id;
};

struct stream_id_descriptor
{
	uint8_t component_tag;
};

/*
 * fills in a struct carousel based on the given service_id
 * returns a ptr to a static struct that will be overwritten be the next call to this routine
 */

static struct carousel _car;

struct carousel *
find_mheg(char *device, unsigned int timeout, uint16_t service_id, int carousel_id)
{
	unsigned char *pat;
	uint16_t section_length;
	uint16_t offset;
	uint16_t map_pid = 0;
	int found;
	unsigned char *pmt;
	uint8_t stream_type;
	uint16_t elementary_pid;
	uint16_t info_length;
	uint8_t desc_tag;
	uint8_t desc_length;
	uint16_t component_tag;

	/* carousel data we know so far */
	_car.device = device;
	_car.timeout = timeout;
	_car.service_id = service_id;

	/* unknown */
	_car.carousel_id = 0;
	/* map between stream_id_descriptors and elementary_PIDs */
	init_assoc(&_car.assoc);
	/* no PIDs yet */
	_car.npids = 0;
	_car.pids = NULL;
	/* no modules loaded yet */
	_car.got_dsi = FALSE;
	_car.nmodules = 0;
	_car.modules = NULL;

	/* get the PAT */
	if((pat = read_table(device, PID_PAT, TID_PAT, timeout)) == NULL)
		fatal("Unable to read PAT");

	section_length = 3 + (((pat[1] & 0x0f) << 8) + pat[2]);

	/* find the PMT for this service_id */
	found = FALSE;
	offset = 8;
	/* -4 for the CRC at the end */
	while((offset < (section_length - 4)) && !found)
	{
		if((pat[offset] << 8) + pat[offset+1] == service_id)
		{
			map_pid = ((pat[offset+2] & 0x1f) << 8) + pat[offset+3];
			found = TRUE;
		}
		else
		{
			offset += 4;
		}
	}

	if(!found)
		fatal("Unable to find programme_map_PID for service_id %u", service_id);

//	printf("programme_map_PID: 0x%x\n", map_pid);

	/* get the PMT */
	if((pmt = read_table(device, map_pid, TID_PMT, timeout)) == NULL)
		fatal("Unable to read PMT");

	section_length = 3 + (((pmt[1] & 0x0f) << 8) + pmt[2]);

	/* skip the program_info descriptors */
	offset = 10;
	info_length = ((pmt[offset] & 0x0f) << 8) + pmt[offset+1];
	offset += 2 + info_length;

	/*
	 *  find the carousel_identification_descriptor
	 *  and the stream_identification_descriptor
	 */
	while(offset < (section_length - 4))
	{
		stream_type = pmt[offset];
		offset += 1;
		elementary_pid = ((pmt[offset] & 0x1f) << 8) + pmt[offset+1];
		offset += 2;
		info_length = ((pmt[offset] & 0x0f) << 8) + pmt[offset+1];
		offset += 2;
		while(info_length != 0)
		{
			desc_tag = pmt[offset];
			desc_length = pmt[offset+1];
			offset += 2;
			info_length -= 2;
			if(desc_tag == TAG_CAROUSEL_ID_DESCRIPTOR)
			{
				struct carousel_id_descriptor *desc;
				desc = (struct carousel_id_descriptor *) &pmt[offset];
				if(carousel_id == -1 || carousel_id == (ntohl(desc->carousel_id)))
				{
					_car.carousel_id = ntohl(desc->carousel_id);
					add_dsmcc_pid(&_car, elementary_pid);
//					printf("pid=0x%x carousel_id=%u\n", elementary_pid, _car.carousel_id);
				}
			}
			else if(desc_tag == TAG_STREAM_ID_DESCRIPTOR)
			{
				struct stream_id_descriptor *desc;
				desc = (struct stream_id_descriptor *) &pmt[offset];
				component_tag = desc->component_tag;
//				printf("pid=0x%x component_tag=0x%x\n", elementary_pid, component_tag);
				add_assoc(&_car.assoc, elementary_pid, desc->component_tag);
			}
			offset += desc_length;
			info_length -= desc_length;
		}
	}

	/* did we find a DSM-CC stream */
	if(_car.npids == 0)
		fatal("Unable to find Carousel Descriptor in PMT");

	return &_car;
}

