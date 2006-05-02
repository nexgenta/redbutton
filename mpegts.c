/*
 * mpegts.c
 */

/*
 * MPEG2 Transport Stream demuxer
 * based on ffmpeg/libavformat code
 * changed to avoid any seeking on the input
 */

/*
 * MPEG2 transport stream (aka DVB) demux
 * Copyright (c) 2002-2003 Fabrice Bellard.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ffmpeg/avformat.h>

#include "mpegts.h"
#include "utils.h"

#define TS_PACKET_SIZE	188
#define NB_PID_MAX	2

typedef struct PESContext PESContext;

struct MpegTSContext
{
	FILE *ts_stream;		/* transport stream we are reading from */
	AVPacket *pkt;			/* packet containing av data */
	int stop_parse;			/* stop parsing loop */
	PESContext *pids[NB_PID_MAX];	/* PIDs we are demuxing */
	int is_start;			/* is the current packet the first of a frame */
};

/* TS stream handling */
enum MpegTSState
{
	MPEGTS_SKIP = 0,	/* must be 0, => don't start getting data until we have set is_start flag */
	MPEGTS_HEADER,
	MPEGTS_PESHEADER_FILL,
	MPEGTS_PAYLOAD,
};

/* enough for PES header + length */
#define PES_START_SIZE		9
#define MAX_PES_HEADER_SIZE	(9 + 255)

struct PESContext
{
	int pid;
	MpegTSContext *ts;
	enum MpegTSState state;
	/* used to get the format */
	int data_index;
	int total_size;
	int pes_header_size;
	int64_t pts, dts;
	uint8_t header[MAX_PES_HEADER_SIZE];
	/* frame we are currently building for this stream */
	int64_t frame_pts;
	int64_t frame_dts;
	uint8_t *frame_data;
	unsigned int frame_size;
};

static int read_packet(FILE *, uint8_t *);
static void handle_packet(MpegTSContext *, const uint8_t *);
static PESContext *add_pes_stream(MpegTSContext *, int);
static void mpegts_push_data(PESContext *, const uint8_t *, int, int);
static int64_t get_pts(const uint8_t *);

/* my interface */

MpegTSContext *
mpegts_open(FILE *ts)
{
	MpegTSContext *ctx;

	if((ctx = av_mallocz(sizeof(MpegTSContext))) == NULL)
		return NULL;

	ctx->ts_stream = ts;

	ctx->is_start = 0;

	return ctx;
}

int
mpegts_demux_frame(MpegTSContext *ctx, AVPacket *frame)
{
	AVPacket packet;
	PESContext *pes;

	do
	{
		mpegts_demux_packet(ctx, &packet);
		/* find the stream */
		if((pes = add_pes_stream(ctx, packet.stream_index)) == NULL)
			fatal("mpegts_demux_frame: internal error");
		/* is it the first packet of the next frame */
		if(ctx->is_start == 0)
		{
			/* not a new frame, add data to the exisiting one */
			pes->frame_data = safe_realloc(pes->frame_data, pes->frame_size + packet.size);
			memcpy(pes->frame_data + pes->frame_size, packet.data, packet.size);
			pes->frame_size += packet.size;
			av_free_packet(&packet);
		}
	}
	while(ctx->is_start == 0);

	/*
	 * pes->frame_data contains the last frame
	 * copy it into the output packet
	 * packet contains the first packet of the next frame
	 * copy it into the PES context for the stream
	 */
	if(av_new_packet(frame, pes->frame_size) != 0)
		return -1;
	memcpy(frame->data, pes->frame_data, pes->frame_size);

	frame->stream_index = pes->pid;
	frame->pts = pes->frame_pts;
	frame->dts = pes->frame_dts;

	/* copy the first packet of the next frame into PES context */
	pes->frame_data = safe_realloc(pes->frame_data, packet.size);
	pes->frame_size = packet.size;
	memcpy(pes->frame_data, packet.data, packet.size);

	/* remember the new frame's PTS (or calc from the previous one) */
	if(packet.pts == AV_NOPTS_VALUE && pes->frame_pts != AV_NOPTS_VALUE)
		pes->frame_pts += 3600;
	else
		pes->frame_pts = packet.pts;
	if(packet.dts == AV_NOPTS_VALUE && pes->frame_dts != AV_NOPTS_VALUE)
		pes->frame_dts += 3600;
	else
		pes->frame_dts = packet.dts;

	av_free_packet(&packet);

	return 0;
}

int
mpegts_demux_packet(MpegTSContext *ctx, AVPacket *pkt)
{
	uint8_t packet[TS_PACKET_SIZE];
	int ret;

	ctx->pkt = pkt;

	ctx->stop_parse = 0;
	for(;;)
	{
		if(ctx->stop_parse)
			break;
		if((ret = read_packet(ctx->ts_stream, packet)) != 0)
			return ret;
		handle_packet(ctx, packet);
	}

	return 0;
}

void
mpegts_close(MpegTSContext *ctx)
{
	int i;

	for(i=0; i<NB_PID_MAX; i++)
	{
		safe_free(ctx->pids[i]->frame_data);
		av_free(ctx->pids[i]);
	}
	av_free(ctx);

	return;
}

/* internal functions */

/* return -1 if error or EOF. Return 0 if OK. */
#define TS_SYNC_BYTE	0x47

static int
read_packet(FILE *ts, uint8_t *buf)
{
	size_t nread;

	/* find the next sync byte */
	nread = 0;
	do
	{
		/* read the whole of the next packet */
		while(nread != TS_PACKET_SIZE && !feof(ts))
			nread += fread(buf + nread, 1, TS_PACKET_SIZE - nread, ts);
		if(*buf != TS_SYNC_BYTE && !feof(ts))
		{
			error("MPEG TS demux: bad sync byte: 0x%02x", *buf);
			memmove(buf, buf + 1, TS_PACKET_SIZE - 1);
			nread = TS_PACKET_SIZE - 1;
		}
	}
	while(*buf != TS_SYNC_BYTE && !feof(ts));

	if(feof(ts))
		return -1;

	return 0;
}

/* handle one TS packet */
static void
handle_packet(MpegTSContext *ctx, const uint8_t *packet)
{
	PESContext *pes;
	int pid, afc;
	const uint8_t *p, *p_end;

	pid = ((packet[1] & 0x1f) << 8) | packet[2];

	if((pes = add_pes_stream(ctx, pid)) == NULL)
		return;

	ctx->is_start = packet[1] & 0x40;

#if 0
	/* continuity check (currently not used) */
	cc = (packet[3] & 0xf);
	cc_ok = (tss->last_cc < 0) || ((((tss->last_cc + 1) & 0x0f) == cc));
	tss->last_cc = cc;
#endif

	/* skip adaptation field */
	afc = (packet[3] >> 4) & 3;
	p = packet + 4;
	if(afc == 0)		/* reserved value */
		return;
	if(afc == 2)		/* adaptation field only */
		return;
	if(afc == 3)		/* skip adapation field */
		p += p[0] + 1;

	/* if past the end of packet, ignore */
	p_end = packet + TS_PACKET_SIZE;
	if(p >= p_end)
		return;

	mpegts_push_data(pes, p, p_end - p, ctx->is_start);

	return;
}

static PESContext *
add_pes_stream(MpegTSContext *ctx, int pid)
{
	PESContext *pes;
	int i;

	/* have we already added this PID */
	for(i=0; i<NB_PID_MAX && ctx->pids[i]!=NULL; i++)
		if(ctx->pids[i]->pid == pid)
			return ctx->pids[i];
	if(i == NB_PID_MAX)
		return NULL;

	/* if no pid found, then add a pid context */
	if((pes = av_mallocz(sizeof(PESContext))) == NULL)
		return NULL;
	pes->ts = ctx;
	pes->pid = pid;

	pes->frame_pts = AV_NOPTS_VALUE;
	pes->frame_dts = AV_NOPTS_VALUE;

	ctx->pids[i] = pes;

	return pes;
}

/* return non zero if a packet could be constructed */
static void
mpegts_push_data(PESContext *pes, const uint8_t *buf, int buf_size, int is_start)
{
	MpegTSContext *ts = pes->ts;
	const uint8_t *p;
	int len, code;

	if(is_start)
	{
		pes->state = MPEGTS_HEADER;
		pes->data_index = 0;
	}
	p = buf;
	while(buf_size > 0)
	{
		switch(pes->state)
		{
		case MPEGTS_HEADER:
			len = PES_START_SIZE - pes->data_index;
			if(len > buf_size)
				len = buf_size;
			memcpy(pes->header + pes->data_index, p, len);
			pes->data_index += len;
			p += len;
			buf_size -= len;
			if(pes->data_index == PES_START_SIZE)
			{
				/* we got all the PES or section header. We can now decide */
				if(pes->header[0] == 0x00
				&& pes->header[1] == 0x00
				&& pes->header[2] == 0x01)
				{
					/* it must be an mpeg2 PES stream */
					code = pes->header[3] | 0x100;
					if(!((code >= 0x1c0 && code <= 0x1df)
					|| (code >= 0x1e0 && code <= 0x1ef)
					|| (code == 0x1bd)))
						goto skip;
					pes->state = MPEGTS_PESHEADER_FILL;
					pes->total_size = (pes->header[4] << 8) | pes->header[5];
					/* NOTE: a zero total size means the PES size is unbounded */
					if (pes->total_size)
						pes->total_size += 6;
					pes->pes_header_size = pes->header[8] + 9;
				}
				else
				{
					/* otherwise, it should be a table */
					/* skip packet */
				skip:
					pes->state = MPEGTS_SKIP;
					continue;
				}
			}
			break;

		case MPEGTS_PESHEADER_FILL:
			/* PES packing parsing */
			len = pes->pes_header_size - pes->data_index;
			if(len > buf_size)
				len = buf_size;
			memcpy(pes->header + pes->data_index, p, len);
			pes->data_index += len;
			p += len;
			buf_size -= len;
			if(pes->data_index == pes->pes_header_size)
			{
				const uint8_t *r;
				unsigned int flags;
				flags = pes->header[7];
				r = pes->header + 9;
				pes->pts = AV_NOPTS_VALUE;
				pes->dts = AV_NOPTS_VALUE;
				if((flags & 0xc0) == 0x80)
				{
					pes->pts = get_pts(r);
					r += 5;
				}
				else if((flags & 0xc0) == 0xc0)
				{
					pes->pts = get_pts(r);
					r += 5;
					pes->dts = get_pts(r);
					r += 5;
				}
				/* we got the full header. We parse it and get the payload */
				pes->state = MPEGTS_PAYLOAD;
			}
			break;

		case MPEGTS_PAYLOAD:
			if(pes->total_size)
			{
				len = pes->total_size - pes->data_index;
				if(len > buf_size)
					len = buf_size;
			}
			else
			{
				len = buf_size;
			}
			if(len > 0)
			{
				AVPacket *pkt = ts->pkt;
				if(av_new_packet(pkt, len) == 0)
				{
					memcpy(pkt->data, p, len);
					pkt->stream_index = pes->pid;
					pkt->pts = pes->pts;
					pkt->dts = pes->dts;
					/* reset pts values */
					pes->pts = AV_NOPTS_VALUE;
					pes->dts = AV_NOPTS_VALUE;
					ts->stop_parse = 1;
					return;
				}
			}
			buf_size = 0;
			break;

		case MPEGTS_SKIP:
			buf_size = 0;
			break;
		}
	}

	return;
}

static int64_t
get_pts(const uint8_t *p)
{
	int64_t pts;
	int val;

	pts = (int64_t)((p[0] >> 1) & 0x07) << 30;
	val = (p[1] << 8) | p[2];
	pts |= (int64_t)(val >> 1) << 15;
	val = (p[3] << 8) | p[4];
	pts |= (int64_t)(val >> 1);

	return pts;
}

