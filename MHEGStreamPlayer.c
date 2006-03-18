/*
 * MHEGStreamPlayer.c
 */

#include <string.h>
#include <stdio.h>

#include "MHEGEngine.h"
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
	p->video_pid = -1;

	return;
}

void
MHEGStreamPlayer_setAudioTag(MHEGStreamPlayer *p, int tag)
{
	if(p->have_audio)
		error("MHEGStreamPlayer: more than one audio stream; only using the last one (%d)", tag);

	p->have_audio = true;
	p->audio_tag = tag;
	p->audio_pid = -1;

	return;
}

void
MHEGStreamPlayer_play(MHEGStreamPlayer *p)
{
	verbose("MHEGStreamPlayer_play: audio_tag=%d video_tag=%d", p->audio_tag, p->video_tag);

	p->audio_pid = p->audio_tag;
	p->video_pid = p->video_tag;
//	if((p->ts = MHEGEngine_openStream(p->have_audio, &p->audio_pid, p->have_video, &p->video_pid)) == NULL)
//		error("Unable to open MPEG stream");
/* TODO */
printf("TODO: MHEGStreamPlayer_play: not yet implemented\n");

	return;
}

void
MHEGStreamPlayer_stop(MHEGStreamPlayer *p)
{
	verbose("MHEGStreamPlayer_stop");

	if(p->ts != NULL)
		fclose(p->ts);

	return;
}

