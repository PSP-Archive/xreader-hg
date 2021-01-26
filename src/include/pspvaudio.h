/*
 * PSP Software Development Kit - http://www.pspdev.org
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 * pspvaudio.h - Prototypes for the sceVaudio library.
 *
 * Copyright (c) 2009 hrimfaxi <outmatch@gmail.com>
 *
 */

#ifndef __SCELIBVAUDIO_H__
#define __SCELIBVAUDIO_H__

#include <psptypes.h>

/** virtual audio sound effect */
enum PspVaudioSFXType
{
	PSP_VAUDIO_FX_TYPE_THRU = 0,
	PSP_VAUDIO_FX_TYPE_HEAVY = 1,
	PSP_VAUDIO_FX_TYPE_POPS = 2,
	PSP_VAUDIO_FX_TYPE_JAZZ = 3,
	PSP_VAUDIO_FX_TYPE_UNIQUE = 4
};

/** virtual audio ALC "Dynamic Normalizer" modes */
enum PspVaudioAlcMode
{
	PSP_VAUDIO_ALC_OFF = 0,
	PSP_VAUDIO_ALC_MODE1 = 1
};

/**
  * Reserve the virtual audio output
  *
  * @param samplecount - The number of samples to output in one output call (can be 512/1024/1152/2048).
  *
  * @param freq - The frequency. One of 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11050, 8000.
  *
  * @param channels - Number of channels. Pass 2 (stereo).
  *
  * @return 0 on success, an error if less than 0.
  */
int sceVaudioChReserve(int samplecount, int freq, int channels);

/**
  * Release the virtual audio output
  *
  * @return 0 on success, an error if less than 0.
  */
int sceVaudioChRelease(void);

/**
  * Output virtual audio
  *
  * @param vol - The volume.
  *
  * @param buf - Pointer to the PCM data to output.
  *
  * @return 0 on success, an error if less than 0.
  */
int sceVaudioOutputBlocking(int vol, void *pbuf);

/**
  * Set virtual audio sound effect
  *
  * @param type - Equalizer type, one of PspVaudioSFXType
  *
  * @param vol - The volume.
  */
int sceVaudioSetEffectType(enum PspVaudioSFXType type, int vol);

/**
  * Set virtual audio ALC mode
  *
  * @param mode - ALC mode, one of PspVaudioAlcMode
  */
int sceVaudioSetAlcMode(int mode);

#endif
