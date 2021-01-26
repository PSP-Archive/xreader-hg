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
 * pspaudiolib.c - Audio library build on top of sceAudio, but to provide
 *                 multiple thread usage and callbacks.
 *
 * Copyright (c) 2005 Adresd
 * Copyright (c) 2005 Marcus R. Brown <mrbrown@ocgnet.org>
 *
 * $Id: xaudiolib.c 58 2008-02-16 08:53:48Z hrimfaxi $
 */

#include <stdlib.h>
#include <string.h>
#include <pspthreadman.h>
#include <pspaudio.h>
#include <malloc.h>

#include "xaudiolib.h"
#include "pspvaudio.h"
#include "conf.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define THREAD_STACK_SIZE (64 * 1024)

static int g_sample_size = PSP_DEFAULT_NUM_AUDIO_SAMPLES;
static bool g_use_vaudio = false;

int setFrequency(unsigned short samples, unsigned short freq, char car)
{
	if (g_use_vaudio)
		return sceVaudioChReserve(samples, freq, car);
	else
		return sceAudioSRCChReserve(samples, freq, car);
}

int xAudioReleaseAudio(void)
{
	if (g_use_vaudio) {
		int ret;

		while ((ret = sceVaudioChRelease()) == 0x80260002);

		return ret;
	} else {
		while (sceAudioOutput2GetRestSample() > 0);
		return sceAudioSRCChRelease();
	}
}

int audioOutpuBlocking(int volume, void *buffer)
{
	if (g_use_vaudio) {
		return sceVaudioOutputBlocking(volume, buffer);
	} else {
		return sceAudioSRCOutputBlocking(volume, buffer);
	}
}

static int audio_ready = 0;
static short audio_sndbuf[PSP_NUM_AUDIO_CHANNELS][2][PSP_MAX_NUM_AUDIO_SAMPLES][2];
static psp_audio_channelinfo AudioStatus[PSP_NUM_AUDIO_CHANNELS];
static volatile int audio_terminate = 0;

void xAudioSetVolume(int channel, int left, int right)
{
	if (channel >= PSP_NUM_AUDIO_CHANNELS)
		return;
	AudioStatus[channel].volumeright = right;
	AudioStatus[channel].volumeleft = left;
}

void xAudioChannelThreadCallback(int channel, void *buf, unsigned int reqn)
{
	xAudioCallback_t callback;

	if (channel >= PSP_NUM_AUDIO_CHANNELS)
		return;

	callback = AudioStatus[channel].callback;
}

void xAudioSetChannelCallback(int channel, xAudioCallback_t callback, void *pdata)
{
	volatile psp_audio_channelinfo *pci;

	if (channel >= PSP_NUM_AUDIO_CHANNELS)
		return;

	pci = &AudioStatus[channel];
	pci->callback = 0;
	pci->pdata = pdata;
	pci->callback = callback;
}

int xAudioOutBlocking(unsigned int channel, unsigned int vol1, unsigned int vol2, void *buf)
{
	if (!audio_ready)
		return -1;
	if (channel >= PSP_NUM_AUDIO_CHANNELS)
		return -1;
	if (vol1 > PSP_VOLUME_MAX)
		vol1 = PSP_VOLUME_MAX;
	if (vol2 > PSP_VOLUME_MAX)
		vol2 = PSP_VOLUME_MAX;
	return audioOutpuBlocking(vol1, buf);
}

static SceUID play_sema = -1;

static int AudioChannelThread(int args, void *argp)
{
	volatile int bufidx = 0;
	int channel = *(int *) argp;

	AudioStatus[channel].threadactive = 1;
	while (audio_terminate == 0) {
		void *bufptr = &audio_sndbuf[channel][bufidx];
		xAudioCallback_t callback;

		callback = AudioStatus[channel].callback;
		if (callback) {
			int ret = callback(bufptr, g_sample_size,
							   AudioStatus[channel].pdata);

			if (ret != 0) {
				break;
			}
		} else {
			unsigned int *ptr = bufptr;
			int i;

			for (i = 0; i < g_sample_size; ++i)
				*(ptr++) = 0;
		}
		//xAudioOutBlocking(channel,AudioStatus[channel].volumeleft,AudioStatus[channel].volumeright,bufptr);
		sceKernelWaitSema(play_sema, 1, 0);
		audioOutpuBlocking(AudioStatus[0].volumeright, bufptr);
		sceKernelSignalSema(play_sema, 1);
		bufidx = (bufidx ? 0 : 1);
	}
	AudioStatus[channel].threadactive = 0;
	sceKernelExitThread(0);
	return 0;
}

int xAudioSetFrequency(unsigned short freq)
{
	int ret = 0;

	switch (freq) {
		case 8000:
		case 12000:
		case 16000:
		case 24000:
		case 32000:
		case 48000:
		case 11025:
		case 22050:
		case 44100:
			break;
		default:
			return -1;
	}
	sceKernelWaitSema(play_sema, 1, 0);
	xAudioReleaseAudio();
	if (setFrequency(g_sample_size, freq, 2) < 0)
		ret = -1;
	sceKernelSignalSema(play_sema, 1);
	return ret;
}

int xAudioInit()
{
	int i, ret;
	int failed = 0;
	char str[32];

	xAudioReleaseAudio();
	audio_terminate = 0;
	audio_ready = 0;
	memset(audio_sndbuf, 0, sizeof(audio_sndbuf));

	xAudioSetUseVaudio(config.use_vaudio);

	if (play_sema < 0) {
		play_sema = sceKernelCreateSema("play_sema", 6, 1, 1, 0);
	}
	for (i = 0; i < PSP_NUM_AUDIO_CHANNELS; i++) {
		AudioStatus[i].handle = -1;
		AudioStatus[i].threadhandle = -1;
		AudioStatus[i].threadactive = 0;
		AudioStatus[i].volumeright = PSP_VOLUME_MAX;
		AudioStatus[i].volumeleft = PSP_VOLUME_MAX;
		AudioStatus[i].callback = 0;
		AudioStatus[i].pdata = 0;
	}
	for (i = 0; i < PSP_NUM_AUDIO_CHANNELS; i++) {
		if (xAudioSetFrequency(44100) < 0)
			failed = 1;
		else
			AudioStatus[i].handle = 0;
	}
	if (failed) {
		for (i = 0; i < PSP_NUM_AUDIO_CHANNELS; i++) {
			if (AudioStatus[i].handle != -1)
				xAudioReleaseAudio();
			AudioStatus[i].handle = -1;
		}
		return -1;
	}
	audio_ready = 1;
	strcpy(str, "audiot0");
	for (i = 0; i < PSP_NUM_AUDIO_CHANNELS; i++) {
		str[6] = '0' + i;
		AudioStatus[i].threadhandle = sceKernelCreateThread(str, (void *) &AudioChannelThread, 0x12, THREAD_STACK_SIZE, PSP_THREAD_ATTR_USER, NULL);
		if (AudioStatus[i].threadhandle < 0) {
			AudioStatus[i].threadhandle = -1;
			failed = 1;
			break;
		}
		ret = sceKernelStartThread(AudioStatus[i].threadhandle, sizeof(i), &i);
		if (ret != 0) {
			failed = 1;
			break;
		}
	}
	if (failed) {
		audio_terminate = 1;
		for (i = 0; i < PSP_NUM_AUDIO_CHANNELS; i++) {
			if (AudioStatus[i].threadhandle != -1) {
				//sceKernelWaitThreadEnd(AudioStatus[i].threadhandle,NULL);
				while (AudioStatus[i].threadactive)
					sceKernelDelayThread(100000);
				sceKernelDeleteThread(AudioStatus[i].threadhandle);
			}
			AudioStatus[i].threadhandle = -1;
		}
		audio_ready = 0;
		return -1;
	}

	if (g_use_vaudio) {
		sceVaudioSetEffectType(config.sfx_mode, 0x8000);
		sceVaudioSetAlcMode(config.alc_mode);
	}

	return 0;
}

void xAudioEndPre()
{
	audio_ready = 0;
	audio_terminate = 1;
}

void xAudioEnd()
{
	int i;

	audio_ready = 0;
	audio_terminate = 1;

	for (i = 0; i < PSP_NUM_AUDIO_CHANNELS; i++) {
		if (AudioStatus[i].threadhandle != -1) {
			xAudioReleaseAudio();
			//sceKernelWaitThreadEnd(AudioStatus[i].threadhandle,NULL);
			while (AudioStatus[i].threadactive)
				sceKernelDelayThread(100000);
			sceKernelDeleteThread(AudioStatus[i].threadhandle);
		}
		AudioStatus[i].threadhandle = -1;
	}

	for (i = 0; i < PSP_NUM_AUDIO_CHANNELS; i++) {
		if (AudioStatus[i].handle != -1) {
			xAudioReleaseAudio();
			AudioStatus[i].handle = -1;
		}
	}
	if (play_sema >= 0) {
		sceKernelDeleteSema(play_sema);
		play_sema = -1;
	}

	g_sample_size = PSP_DEFAULT_NUM_AUDIO_SAMPLES;
}

/**
 * 清空声音缓冲区
 *
 * @param buf 声音缓冲区指针
 * @param frames 帧数大小
 */
void xAudioClearSndBuf(void *buf, int frames)
{
	memset(buf, 0, frames * 2 * 2);
}

void *xAudioAlloc(size_t align, size_t bytes)
{
	void *p = memalign(align, bytes);

	if (p)
		memset(p, 0, bytes);

	return p;
}

void xAudioFree(void *p)
{
	free(p);
}

void xAudioSetFrameSize(int size)
{
	if (size <= PSP_MAX_NUM_AUDIO_SAMPLES)
		g_sample_size = size;
}

void xAudioSetUseVaudio(unsigned char use_vaudio)
{
	g_use_vaudio = use_vaudio;
}

void xAudioSetEffectType(int type)
{
	if (g_use_vaudio)
		sceVaudioSetEffectType(type, 0x8000);
}

void xAudioSetAlcMode(unsigned char mode)
{
	if (g_use_vaudio)
		sceVaudioSetAlcMode(mode);
}
