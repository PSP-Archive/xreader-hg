#ifndef _WIN32

#ifndef APE_NOWINDOWS_H
#define APE_NOWINDOWS_H

#define __forceinline inline

#define FALSE    0
#define TRUE     1

#define NEAR
#define FAR

#include <wchar.h>

enum {
	ERROR_INVALID_PARAMETER = -87,
};

#define LPCTSTR const char *
#define LPCWSTR wchar_t *

#define _wcsicmp wcscasecmp
#define _wcsnicmp wcsncasecmp
#define _wtoi wtoi

int wcscasecmp(const wchar_t *s1, const wchar_t *s2);
int wcsncasecmp(const wchar_t *s1, const wchar_t *s2, size_t n);
int wtoi(const wchar_t *);

typedef unsigned char UCHAR;

typedef unsigned int        uint32;
typedef int                 int32;
typedef unsigned short      uint16;
typedef short               int16;
typedef unsigned char       uint8;
typedef char                int8;
typedef char                str_ansi;
typedef unsigned char       str_utf8;
typedef wchar_t             str_utf16;

typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;
typedef void *              HANDLE;
typedef unsigned int        UINT;
typedef unsigned int        WPARAM;
typedef long                LPARAM;
typedef const char *        LPCSTR;
typedef char *              LPSTR;
typedef long                LRESULT;

#define ZeroMemory(POINTER, BYTES) memset(POINTER, 0, BYTES);
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#define min(a,b)    (((a) < (b)) ? (a) : (b))

#define __stdcall
#define CALLBACK

#define _stricmp strcasecmp
#define _strnicmp strncasecmp

#define _FPOSOFF(fp) ((int)(fp).__pos)
#define MAX_PATH    260

#ifndef _WAVEFORMATEX_
#define _WAVEFORMATEX_

typedef struct tWAVEFORMATEX
{
    WORD        wFormatTag;         /* format type */
    WORD        nChannels;          /* number of channels (i.e. mono, stereo...) */
    DWORD       nSamplesPerSec;     /* sample rate */
    DWORD       nAvgBytesPerSec;    /* for buffer estimation */
    WORD        nBlockAlign;        /* block size of data */
    WORD        wBitsPerSample;     /* number of bits per sample of mono data */
    WORD        cbSize;             /* the count in bytes of the size of */
                    /* extra information (after cbSize) */
} WAVEFORMATEX, *PWAVEFORMATEX, NEAR *NPWAVEFORMATEX, FAR *LPWAVEFORMATEX;
typedef const WAVEFORMATEX FAR *LPCWAVEFORMATEX;

#endif // #ifndef _WAVEFORMATEX_

#endif // #ifndef APE_NOWINDOWS_H

#endif // #ifndef _WIN32
