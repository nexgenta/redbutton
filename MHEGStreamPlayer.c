/*
 * MHEGStreamPlayer.c
 */

#include <string.h>
#include <stdio.h>

#include "MHEGStreamPlayer.h"
#include "utils.h"

void
MHEGStreamPlayer_init(MHEGStreamPlayer *p)
{
	bzero(p, sizeof(MHEGStreamPlayer));

	p->have_video = false;
	p->have_audio = false;

	return;
}

void
MHEGStreamPlayer_fini(MHEGStreamPlayer *p)
{
	MHEGStreamPlayer_stop(p);

	return;
}

void
MHEGStreamPlayer_setVideoTag(MHEGStreamPlayer *p, int tag)
{
	if(p->have_video)
		error("MHEGStreamPlayer: more than one video stream; only using the last one (%d)", tag);

	p->have_video = true;
	p->video_tag = tag;

	return;
}

void
MHEGStreamPlayer_setAudioTag(MHEGStreamPlayer *p, int tag)
{
	if(p->have_audio)
		error("MHEGStreamPlayer: more than one audio stream; only using the last one (%d)", tag);

	p->have_audio = true;
	p->audio_tag = tag;

	return;
}

void
MHEGStreamPlayer_play(MHEGStreamPlayer *p)
{
/* TODO */
printf("TODO: MHEGStreamPlayer_play: not yet implemented\n");
if(p->have_audio) printf("TODO: audio tag=%d\n", p->audio_tag);
if(p->have_video) printf("TODO: video tag=%d\n", p->video_tag);

	return;
}

void
MHEGStreamPlayer_stop(MHEGStreamPlayer *p)
{
/* TODO */
printf("TODO: MHEGStreamPlayer_stop: not yet implemented\n");

	return;
}

