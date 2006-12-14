/*
 * table.c
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

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(HAVE_DREAMBOX_HARDWARE)
#include <ost/dmx.h>
#define dmx_sct_filter_params dmxSctFilterParams
#else
#include <linux/dvb/dmx.h>
#endif

#include "dsmcc.h"
#include "carousel.h"
#include "biop.h"
#include "utils.h"

#define MAX_TABLE_LEN	4096

/* DSMCC table ID's we want */
#define TID_DSMCC_CONTROL	0x3b	/* DSI or DII */
#define TID_DSMCC_DATA		0x3c	/* DDB */

static unsigned char _buf[MAX_TABLE_LEN];

unsigned char *
read_table(char *device, uint16_t pid, uint8_t tid, unsigned int secs)
{
	int fd_data;
	struct dmx_sct_filter_params sctFilterParams;
	fd_set readfds;
	struct timeval timeout;
	int n;

	if((fd_data = open(device, O_RDWR)) < 0)
	{
		error("open '%s': %s", device, strerror(errno));
		return NULL;
	}

	memset(&sctFilterParams, 0, sizeof(sctFilterParams));
	sctFilterParams.pid = pid;
	sctFilterParams.timeout = 0;
	sctFilterParams.flags = DMX_IMMEDIATE_START;
        sctFilterParams.filter.filter[0] = tid;
 	sctFilterParams.filter.mask[0] = 0xff;

	if(ioctl(fd_data, DMX_SET_FILTER, &sctFilterParams) < 0)
	{
		error("ioctl DMX_SET_FILTER: %s", strerror(errno));
		close(fd_data);
		return NULL;
	}

	timeout.tv_sec = secs;
	timeout.tv_usec = 0;
	do
	{
		_buf[0] = 0;
		FD_ZERO(&readfds);
		FD_SET(fd_data, &readfds);
		if(select(fd_data + 1, &readfds, NULL, NULL, &timeout) < 0)
		{
			error("select: %s", strerror(errno));
			close(fd_data);
			return NULL;
		}
		if(FD_ISSET(fd_data, &readfds))
		{
			if((n = read(fd_data, _buf, sizeof(_buf))) < 0)
			{
				error("read: %s", strerror(errno));
				close(fd_data);
				return NULL;
			}
		}
		else
		{
			error("Timeout reading %s", device);
			close(fd_data);
			return NULL;
		}
	}
	while(_buf[0] != tid);

//	printf("PID: 0x%x table_id: 0x%x length: %d bytes\n", pid, _buf[0], n);
//	hexdump(_buf, n);

	close(fd_data);

	return _buf;
}

unsigned char *
read_dsmcc_tables(struct carousel *car)
{
	struct timeval timeout;
	unsigned int i;
	fd_set readfds;
	int max;
	int fd;
	int n;

	timeout.tv_sec = car->timeout;
	timeout.tv_usec = 0;
	do
	{
		_buf[0] = 0;
		/* work out the max fd number and set the fds we want to read from */
		max = 0;
		FD_ZERO(&readfds);
		for(i=0; i<car->npids; i++)
		{
			max = MAX(max, car->pids[i].fd_ctrl);
			max = MAX(max, car->pids[i].fd_data);
			FD_SET(car->pids[i].fd_ctrl, &readfds);
			FD_SET(car->pids[i].fd_data, &readfds);
		}
		/* wait for some data to be ready */
		if(select(max + 1, &readfds, NULL, NULL, &timeout) < 0)
		{
			error("select: %s", strerror(errno));
			return NULL;
		}
		/* see which fd is ready */
		fd = -1;
		for(i=0; fd==-1 && i<car->npids; i++)
		{
			if(FD_ISSET(car->pids[i].fd_ctrl, &readfds))
			{
				fd = car->pids[i].fd_ctrl;
				/* remember where we got the data from */
				car->current_pid = car->pids[i].pid;
			}
			else if(FD_ISSET(car->pids[i].fd_data, &readfds))
			{
				fd = car->pids[i].fd_data;
				/* remember where we got the data from */
				car->current_pid = car->pids[i].pid;
			}
		}
		if(fd == -1)
		{
			error("Timeout reading %s", car->demux_device);
			return NULL;
		}
		/* read the table */
		if((n = read(fd, _buf, sizeof(_buf))) < 0)
		{
			/*
			 * may get EOVERFLOW if we don't read quick enough,
			 * so just report it and have another go
			 */
			error("read: %s", strerror(errno));
			_buf[0] = 0;
		}
	}
	while(_buf[0] != TID_DSMCC_CONTROL && _buf[0] != TID_DSMCC_DATA);

//	printf("PID: 0x%x table_id: 0x%x length: %d bytes\n", car->current_pid, _buf[0], n);
//	hexdump(_buf, n);

	return _buf;
}

void
add_dsmcc_pid(struct carousel *car, uint16_t pid)
{
	unsigned int i;
	struct pid_fds *fds;
	struct dmx_sct_filter_params sctFilterParams;

	/* make sure we haven't added it already */
	for(i=0; i<car->npids; i++)
		if(car->pids[i].pid == pid)
			return;

	verbose("Adding PID %u to filter", pid);

	/* add a new PID data structure */
	car->npids ++;
	car->pids = safe_realloc(car->pids, car->npids * sizeof(struct pid_fds));
	fds = &car->pids[car->npids - 1];

	fds->pid = pid;

	/* open an fd to read the DSMCC control tables (DSI and DII) */
	if((fds->fd_ctrl = open(car->demux_device, O_RDWR)) < 0)
		fatal("open '%s': %s", car->demux_device, strerror(errno));

	/* set the table filter */
	memset(&sctFilterParams, 0, sizeof(sctFilterParams));
	sctFilterParams.pid = pid;
	sctFilterParams.timeout = 0;
	sctFilterParams.flags = DMX_IMMEDIATE_START;
	sctFilterParams.filter.filter[0] = TID_DSMCC_CONTROL;
	sctFilterParams.filter.mask[0] = 0xff;
	if(ioctl(fds->fd_ctrl, DMX_SET_FILTER, &sctFilterParams) < 0)
		fatal("ioctl DMX_SET_FILTER: %s", strerror(errno));

	/* open an fd to read the DSMCC data table (DDB) */
	if((fds->fd_data = open(car->demux_device, O_RDWR)) < 0)
		fatal("open '%s': %s", car->demux_device, strerror(errno));

	/* set the table filter */
	memset(&sctFilterParams, 0, sizeof(sctFilterParams));
	sctFilterParams.pid = pid;
	sctFilterParams.timeout = 0;
	sctFilterParams.flags = DMX_IMMEDIATE_START;
	sctFilterParams.filter.filter[0] = TID_DSMCC_DATA;
	sctFilterParams.filter.mask[0] = 0xff;
	if(ioctl(fds->fd_data, DMX_SET_FILTER, &sctFilterParams) < 0)
		fatal("ioctl DMX_SET_FILTER: %s", strerror(errno));

	return;
}

