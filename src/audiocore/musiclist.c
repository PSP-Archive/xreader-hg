#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include "musiclist.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

static MusicListEntry *musiclist_get_prev(MusicList * list, MusicListEntry * p);

MusicList g_music_list;

size_t musiclist_count(MusicList * list)
{
	return list->count;
}

void musiclist_init(MusicList * list)
{
	memset(&list->head, 0, sizeof(list->head));
	list->tail = &list->head;
}

int musiclist_add(MusicList * list, const char *spath, const char *lpath)
{
	MusicListEntry *p;
	char *newspath, *newlpath;

	p = (MusicListEntry *) malloc(sizeof(*p));

	if (p == NULL) {
		return -1;
	}

	newspath = strdup(spath);

	if (newspath == NULL) {
		free(p);

		return -2;
	}

	newlpath = strdup(lpath);

	if (newlpath == NULL) {
		free(p);
		free(newspath);

		return -3;
	}

	p->spath = newspath;
	p->lpath = newlpath;
	p->shuffle_accessed = 0;

	list->tail->next = p;
	list->tail = p;
	list->tail->next = NULL;
	list->count++;

	return 0;
}

int musiclist_remove(MusicList * list, MusicListEntry * entry)
{
	MusicListEntry *prev, *p;

	if(entry == NULL) {
		return -1;
	}

	for (prev = &list->head, p = list->head.next; p != NULL; prev = p, p = p->next) {
		if (p == entry) {
			break;
		}
	}

	if (p == NULL) {
		return -1;
	}

	if (list->tail == p) {
		list->tail = prev;
	}

	prev->next = entry->next;
	free(entry->spath);
	free(entry->lpath);
	free(entry);

	list->count--;

	return 0;
}

int musiclist_isempty(MusicList * list)
{
	return list->tail == &list->head ? 1 : 0;
}

int musiclist_clear(MusicList * list)
{
	while (!musiclist_isempty(list)) {
		musiclist_remove(list, list->tail);
	}

	return 0;
}

MusicListEntry *musiclist_get(MusicList * list, size_t idx)
{
	MusicListEntry *p;
	size_t i;

	for (i = 0, p = list->head.next; p != NULL && i < idx; p = p->next, ++i) {
	}

	return p;
}

void musiclist_begin_shuffle(MusicList * list)
{
	MusicListEntry *p;

	for (p = list->head.next; p != NULL; p = p->next) {
		p->shuffle_accessed = 0;
	}
}

MusicListEntry *musiclist_get_next_shuffle(MusicList * list)
{
	MusicListEntry *p, *q;
	int cnt;

	q = NULL;
	for (cnt = 1, p = list->head.next, q = NULL; p != NULL; p = p->next) {
		if (!p->shuffle_accessed) {
			if ((rand() % cnt++) == 0) {
				q = p;
			}
		}
	}

	if (q != NULL) {
		q->shuffle_accessed = 1;
	}

	return q;
}

int musiclist_get_idx(MusicList * list, MusicListEntry * entry)
{
	MusicListEntry *p;
	size_t i;

	for (i = 0, p = list->head.next; p != NULL; p = p->next, ++i) {
		if (p == entry) {
			break;
		}
	}

	if (p == NULL) {
		return -1;
	}

	return i;
}

MusicListEntry *musiclist_search_by_path(MusicList * list, const char *spath, const char *lpath)
{
	MusicListEntry *p;

	for (p = list->head.next; p != NULL; p = p->next) {
		if (0 == strcmp(spath, p->spath) && 0 == strcmp(lpath, p->lpath)) {
			break;
		}
	}

	return p;
}

int musiclist_moveup(MusicList * list, int i)
{
	MusicListEntry *p, *prev, *prev2, *q;

	p = musiclist_get(list, i);

	if (p == NULL) {
		return -1;
	}

	prev = musiclist_get_prev(list, p);

	if (prev == &list->head) {
		return 0;
	}

	prev2 = musiclist_get_prev(list, prev);

	q = p->next;
	prev2->next = p;
	prev2->next->next = prev;
	prev2->next->next->next = q;

	if (list->tail == p) {
		list->tail = p->next;
	}

	return 0;
}

int musiclist_movedown(MusicList * list, int i)
{
	MusicListEntry *p, *q, *prev;

	p = musiclist_get(list, i);

	if (p == NULL) {
		return -1;
	}

	if (list->tail == p) {
		return 0;
	}

	q = p->next->next;
	prev = musiclist_get_prev(list, p);

	if (list->tail == p->next) {
		list->tail = p;
	}

	prev->next = p->next;
	prev->next->next = p;
	prev->next->next->next = q;

	return 0;
}

static MusicListEntry *musiclist_get_prev(MusicList * list, MusicListEntry * p)
{
	MusicListEntry *q;

	if (p == NULL) {
		return NULL;
	}

	for (q = &list->head; q != NULL; q = q->next) {
		if (q->next == p) {
			break;
		}
	}

	return q;
}
