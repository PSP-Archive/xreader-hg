#ifndef MUSICLIST_H
#define MUSICLIST_H

typedef struct _MusicListEntry {
	char *spath;
	char *lpath;
	int shuffle_accessed;
	struct _MusicListEntry *next;
} MusicListEntry;

typedef struct _MusicList {
	size_t count;
	MusicListEntry head, *tail;
} MusicList;

extern MusicList g_music_list;

size_t musiclist_count(MusicList *list);
void musiclist_init(MusicList *list);
int musiclist_add(MusicList *list, const char *spath, const char *lpath);
int musiclist_remove(MusicList *list, MusicListEntry *entry);
int musiclist_isempty(MusicList *list);
int musiclist_clear(MusicList *list);
MusicListEntry *musiclist_get(MusicList *list, size_t idx);
int musiclist_get_idx(MusicList *list, MusicListEntry *entry);
MusicListEntry *musiclist_search_by_path(MusicList *list, const char *spath, const char *lpath);
int musiclist_moveup(MusicList *list, int i);
int musiclist_movedown(MusicList *list, int i);
void musiclist_begin_shuffle(MusicList *list);
MusicListEntry *musiclist_get_next_shuffle(MusicList *list);

#endif
