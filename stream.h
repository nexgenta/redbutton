/*
 * stream.h
 */

#ifndef __STREAM_H__
#define __STREAM_H__

#include <stdint.h>
#include <linux/dvb/dmx.h>

int add_demux_filter(char *, uint16_t, dmx_pes_type_t);

void stream_ts(int, int);

#endif	/* __STREAM_H__ */
