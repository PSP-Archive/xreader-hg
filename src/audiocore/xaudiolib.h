/*
 * This file is part of xReader.
 *
 * Copyright (C) 2008 hrimfaxi (outmatch@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*
 * PSP Software Development Kit - http://www.pspdev.org
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 * pspaudiolib.h - Audio library build on top of sceAudio, but to provide
 *                 multiple thread usage and callbacks.
 *
 * Copyright (c) 2005 Adresd
 * Copyright (c) 2005 Marcus R. Brown <mrbrown@ocgnet.org>
 *
 * $Id: xaudiolib.h 58 2008-02-16 08:53:48Z hrimfaxi $
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

//I need just one channel, more CPU free:
#define PSP_NUM_AUDIO_CHANNELS 1
//#define PSP_NUM_AUDIO_CHANNELS 4

/** This is the number of frames you can update per callback, a frame being
 * 1 sample for mono, 2 samples for stereo etc. */
#define PSP_DEFAULT_NUM_AUDIO_SAMPLES (1152)
#define PSP_MAX_NUM_AUDIO_SAMPLES (1024*4)
#define PSP_VOLUME_MAX 0x8000

	typedef int (*xAudioCallback_t) (void *buf, unsigned int reqn, void *pdata);

	typedef struct
	{
		int threadhandle;
		int handle;
		int volumeleft;
		int volumeright;
		xAudioCallback_t callback;
		void *pdata;
		int threadactive;
	} psp_audio_channelinfo;

	typedef int (*xAudioThreadfunc_t) (int args, void *argp);

	int xAudioInit();
	void xAudioEndPre();
	void xAudioEnd();

	void xAudioSetVolume(int channel, int left, int right);
	void xAudioChannelThreadCallback(int channel, void *buf, unsigned int reqn);
	void xAudioSetChannelCallback(int channel, xAudioCallback_t callback, void *pdata);
	int xAudioOutBlocking(unsigned int channel, unsigned int vol1, unsigned int vol2, void *buf);
	int xAudioSetFrequency(unsigned short freq);

	void xAudioClearSndBuf(void *buf, int frames);
	void *xAudioAlloc(size_t align, size_t bytes);
	void xAudioFree(void *p);
	void xAudioSetFrameSize(int size);
	void xAudioSetUseVaudio(unsigned char use_vaudio);
	void xAudioSetEffectType(int type);
	void xAudioSetAlcMode(unsigned char mode);

#ifdef __cplusplus
}
#endif
