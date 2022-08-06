#ifndef tilecache_h
#define tilecache_h

#include <stdint.h>
#include <curl/curl.h>
#include <stdlib.h>
#include "config.h"

#define MAX_PARALLEL_CONNS 2

typedef struct {
	int cur_tilecount, max_tilecount;
	CURL *curl_handle;
} TileCache;


void tilecache_init(TileCache *cache, int max_tilecount);
void tilecache_deinit(TileCache *cache);
void tilecache_set_capacity(TileCache *cache, int max_tilecount);

void tilecache_flush(TileCache *cache);
const uint8_t* tilecache_get(const Config *config, TileCache *cache, size_t *len, int x, int y, int zoom);


#endif
