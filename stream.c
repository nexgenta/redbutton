/*
 * stream.c
 */

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "stream.h"
#include "utils.h"

int
add_demux_filter(char *demux_dev, uint16_t pid, dmx_pes_type_t pes_type)
{
	int fd;
	struct dmx_pes_filter_params pes_filter;

	if((fd = open(demux_dev, O_RDWR)) < 0)
	{
		error("open '%s': %s", demux_dev, strerror(errno));
		return -1;
	}

	bzero(&pes_filter, sizeof(pes_filter));
	pes_filter.input = DMX_IN_FRONTEND;
	pes_filter.output = DMX_OUT_TS_TAP;
	pes_filter.flags = DMX_IMMEDIATE_START;
	pes_filter.pid = pid;
	pes_filter.pes_type = pes_type;

	if(ioctl(fd, DMX_SET_PES_FILTER, &pes_filter) < 0)
	{
		error("ioctl DMX_SET_PES_FILTER: %s", strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
}

/* don't want it on the stack */
static unsigned char _ts_buf[8 * 1024];

void
stream_ts(int ts_fd, int client_sock)
{
	ssize_t nread;
	ssize_t nwritten;

	do
	{
		nread = read(ts_fd, _ts_buf, sizeof(_ts_buf));
		if(nread > 0)
			nwritten = write(client_sock, _ts_buf, nread);
		else
			nwritten = nread;
	}
	while(nread == nwritten);

	return;
}

