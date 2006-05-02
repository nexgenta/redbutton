/*
 * mpegts.h
 */

#ifndef __MPEGTS_H__
#define __MPEGTS_H__

typedef struct MpegTSContext MpegTSContext;

MpegTSContext *mpegts_open(FILE *);
int mpegts_demux_frame(MpegTSContext *, AVPacket *);
int mpegts_demux_packet(MpegTSContext *, AVPacket *);
void mpegts_close(MpegTSContext *);

#endif	/* __MPEGTS_H__ */
