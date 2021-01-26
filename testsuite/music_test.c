#include <pspdebug.h>
#include <pspkernel.h>
#include <pspsdk.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "dbg.h"
#include "conf.h"
#include "freq_lock.h"
#include "music_test.h"
#include "commons.h"
#include "common/utils.h"
#include "charsets.h"
#include "fat.h"
#include "strsafe.h"

#ifdef ENABLE_MUSIC
#include "musicmgr.h"
#include "musicdrv.h"
#else
#error ENABLE_MUSIC not found
#endif

int music_test(void)
{
	char mp3conf[512];

	fat_init();
	ctrl_init();
	music_init();

	strcpy(mp3conf, "ms0:/psp/game/xReader/music.lst");

	if (music_list_load(mp3conf) != 0) {
		music_add_dir("ms0:/MUSIC/", "ms0:/MUSIC/");
	}

	music_set_hprm(true);
	music_set_cycle_mode(config.mp3cycle);
	music_load(0);

	char oldtag[512];

	oldtag[0] = '\0';

	while (1) {
		struct music_info info = { 0 };

		info.type = MD_GET_TITLE | MD_GET_ARTIST | MD_GET_ALBUM;

		if (musicdrv_get_info(&info) == 0) {
			char tag[512];

			switch (info.encode) {
				case conf_encode_utf8:
					charsets_utf8_conv((const byte *) info.artist,
									   sizeof(info.artist),
									   (byte *) info.artist,
									   sizeof(info.artist));
					charsets_utf8_conv((const byte *) info.title,
									   sizeof(info.title), (byte *) info.title,
									   sizeof(info.title));
					charsets_utf8_conv((const byte *) info.album,
									   sizeof(info.album), (byte *) info.album,
									   sizeof(info.album));
					break;
				case conf_encode_big5:
					charsets_big5_conv((const byte *) info.artist,
									   sizeof(info.artist),
									   (byte *) info.artist,
									   sizeof(info.artist));
					charsets_big5_conv((const byte *) info.title,
									   sizeof(info.title), (byte *) info.title,
									   sizeof(info.title));
					charsets_big5_conv((const byte *) info.album,
									   sizeof(info.album), (byte *) info.album,
									   sizeof(info.album));
					break;
				case conf_encode_sjis:
					{
						byte *temp;
						dword size;

						temp = NULL, size = strlen(info.artist);
						charsets_sjis_conv((const byte *) info.artist,
										   (byte **) & temp, (dword *) & size);
						strncpy_s(info.artist, sizeof(info.artist),
								  (const char *) temp, size);
						free(temp);
						temp = NULL, size = strlen(info.title);
						charsets_sjis_conv((const byte *) info.title,
										   (byte **) & temp, (dword *) & size);
						strncpy_s(info.title, sizeof(info.title),
								  (const char *) temp, size);
						free(temp);
						temp = NULL, size = strlen(info.album);
						charsets_sjis_conv((const byte *) info.album,
										   (byte **) & temp, (dword *) & size);
						strncpy_s(info.album, sizeof(info.album),
								  (const char *) temp, size);
						free(temp);
					}
					break;
				case conf_encode_gbk:
					break;
				default:
					break;
			}

			if (info.artist[0] != '\0' && info.album[0] != '\0'
				&& info.title[0] != '\0')
				SPRINTF_S(tag, "%s - %s - %s", info.artist, info.album,
						  info.title);
			else if (info.album[0] != '\0' && info.title[0] != '\0')
				SPRINTF_S(tag, "%s - %s", info.album, info.title);
			else if (info.artist[0] != '\0' && info.title[0] != '\0')
				SPRINTF_S(tag, "%s - %s", info.artist, info.title);
			else if (info.title[0] != '\0')
				SPRINTF_S(tag, "%s", info.title);
			else {
				int i = music_get_current_pos();
				struct music_file *fl = music_get(i);

				if (fl) {
					const char *p = strrchr(fl->longpath, '/');

					if (p == NULL || *(p + 1) == '\0') {
						SPRINTF_S(tag, "%s", fl->longpath);
					} else {
						SPRINTF_S(tag, "%s", p + 1);
					}
				} else
					STRCPY_S(tag, "");
			}

			if (strcmp(oldtag, tag)) {
				dbg_printf(d, "%s", tag);
				strcpy(oldtag, tag);
			}
		}

		sceKernelDelayThread(1000000);
	}

	return 0;
}
