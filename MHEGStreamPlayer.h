/*
 * MHEGStreamPlayer.h
 */

#ifndef __MHEGSTREAMPLAYER_H__
#define __MHEGSTREAMPLAYER_H__

#include <stdbool.h>

typedef struct
{
	bool have_video;	/* false if we have no video stream */
	bool have_audio;	/* false if we have no audio stream */
	int video_tag;		/* video stream component tag (-1 => default for current service ID) */
	int audio_tag;		/* audio stream component tag (-1 => default for current service ID) */
} MHEGStreamPlayer;

void MHEGStreamPlayer_init(MHEGStreamPlayer *);
void MHEGStreamPlayer_fini(MHEGStreamPlayer *);

void MHEGStreamPlayer_setVideoTag(MHEGStreamPlayer *, int);
void MHEGStreamPlayer_setAudioTag(MHEGStreamPlayer *, int);

void MHEGStreamPlayer_play(MHEGStreamPlayer *);
void MHEGStreamPlayer_stop(MHEGStreamPlayer *);

#endif	/* __MHEGSTREAMPLAYER_H__ */
