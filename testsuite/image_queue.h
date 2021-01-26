#ifndef IMAGE_QUEUE_H
#define IMAGE_QUEUE_H

enum
{
	CACHE_INIT = 0,
	CACHE_OK = 1,
	CACHE_FAILED = 2
};

typedef struct _cache_image_t
{
	const char *archname;
	const char *filename;
	int where;
	int status;
	pixel *data;
	pixel bgc;
	dword width;
	dword height;
	int result;
	dword selidx;
	dword filesize;
} cache_image_t;

typedef struct _cacher_context
{
	bool on;
	bool first_run;
	bool isforward;
	dword memory_usage;
	bool selidx_moved;

	cache_image_t *caches;
	size_t caches_cap, caches_size;
	SceUID cacher_locker, cacher_thread;
} cacher_context;

int cache_init(unsigned max_cache_img, dword * c_selidx);
void cache_free(void);
void dbg_dump_cache(void);
int cache_get_size();
void cache_set_forward(bool forward);
void cache_next_image(void);
void cache_on(bool on);
int cache_delete_first(void);
int cache_get_loaded_size();

int image_queue_test(void);

extern volatile cacher_context ccacher;

#endif
