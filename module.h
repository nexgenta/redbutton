/*
 * module.h
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

#ifndef __MODULE_H__
#define __MODULE_H__

#include <stdint.h>
#include <stdbool.h>

#include "dsmcc.h"
#include "assoc.h"

/* PIDs we are reading */
struct pid_fds
{
	uint16_t pid;		/* DVB programme ID */
	int fd_ctrl;		/* fd for reading DSMCC control table (0x3b) */
	int fd_data;		/* fd for reading DSMCC data table (0x3c) */
};

/* data about each module */
struct module
{
	uint16_t module_id;
	uint32_t download_id;
	uint8_t version;
	uint16_t block_size;
	uint16_t nblocks;
	uint32_t blocks_left;		/* number of blocks left to download */
	bool *got_block;		/* which blocks we have downloaded so far */
	uint32_t size;			/* size of the file */
	unsigned char *data;		/* the actual file data */
};

/* the whole carousel */
struct carousel
{
	char *device;			/* demux device */
	unsigned int timeout;		/* timeout for the demux device */
	uint16_t service_id;
	uint32_t carousel_id;
	uint16_t audio_pid;		/* PID of default audio stream for this service_id */
	uint16_t video_pid;		/* PID of default video stream for this service_id */
	uint16_t current_pid;		/* PID we downloaded the last table from */
	struct assoc assoc;		/* map stream_id's to elementary_pid's */
	int32_t npids;			/* PIDs we are reading data from */
	struct pid_fds *pids;		/* array, npids in length */
	bool got_dsi;			/* true if we have downloaded the DSI */
	uint32_t nmodules;		/* modules we have/are downloading */
	struct module *modules;		/* array, nmodules in length */
};

/* functions */
struct module *find_module(struct carousel *, uint16_t, uint8_t, uint32_t);
struct module *add_module(struct carousel *, struct DownloadInfoIndication *, struct DIIModule *);
void delete_module(struct carousel *, uint32_t);
void free_module(struct module *);
void download_block(struct carousel *, struct module *, uint16_t, unsigned char *, uint32_t);

int uncompress_module(struct module *);

#endif	/* __MODULE_H__ */

