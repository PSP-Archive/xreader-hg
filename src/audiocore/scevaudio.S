   .set noreorder

#include "pspimport.s"

IMPORT_START  "sceVaudio",0x00090000
IMPORT_FUNC   "sceVaudio",0x8986295E,sceVaudioOutputBlocking
IMPORT_FUNC   "sceVaudio",0x03B6807D,sceVaudioChReserve
IMPORT_FUNC   "sceVaudio",0x67585DFD,sceVaudioChRelease
IMPORT_FUNC   "sceVaudio",0x346fbe94,sceVaudioSetEffectType
IMPORT_FUNC   "sceVaudio",0xcbd4ac51,sceVaudioSetAlcMode
