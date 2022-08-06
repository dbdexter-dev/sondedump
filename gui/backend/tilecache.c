#include <assert.h>
#include "compat/threads.h"
#include "compat/mutex.h"
#include "compat/semaphore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log/log.h"
#include "tilecache.h"
#include "utils.h"
#include "gui/gui.h"

#ifdef _WIN32
#include <shlobj.h>
#define DIR_SEP "\\"
#elif defined(__APPLE__)
#include <CoreServices/CoreServices.h>
#include <sys/stat.h>
#define DIR_SEP "/"
#elif defined(__linux__)
#include <sys/stat.h>
#define DIR_SEP "/"
#endif

struct transfers {
	int x, y, zoom;
	char *path;
	FILE *fd;
	CURL *handle;

	struct transfers *next;
};


static CURL* add_download(const char *url, FILE *fd);
static void tile_path(char *buf, int x, int y, int zoom);
static void endpoint_path(char *buf, const char *base_url, int x, int y, int zoom);
static void transfers_deinit(TileCache *cache, struct transfers **_xfers);

static void* download_thr(void *ptr);


static thread_t _tid;
static mutex_t _xfers_mutex;
static semaphore_t _xfers_sem;
static struct transfers *_xfers;
static int _interrupted;


void
tilecache_init(TileCache *cache, int max_tilecount)
{
	cache->cur_tilecount = 0;
	cache->max_tilecount = max_tilecount;
	cache->curl_handle = curl_multi_init();
	curl_multi_setopt(cache->curl_handle, CURLMOPT_MAX_TOTAL_CONNECTIONS, MAX_PARALLEL_CONNS);

	_xfers = NULL;
	_interrupted = 0;
	semaphore_init(&_xfers_sem, 0);
	mutex_init(&_xfers_mutex);
	_tid = thread_create(download_thr, cache);
}

void
tilecache_deinit(TileCache *cache)
{
	/* Unlock worker and cause it to exit */
	_interrupted = 1;
	semaphore_post(&_xfers_sem);
	thread_join(_tid);

	/* Deintialize */
	mutex_lock(&_xfers_mutex);
	transfers_deinit(cache, &_xfers);
	mutex_unlock(&_xfers_mutex);

	mutex_destroy(&_xfers_mutex);
	semaphore_destroy(&_xfers_sem);
	curl_multi_cleanup(cache->curl_handle);

}

void
tilecache_flush(TileCache *cache)
{
	char path[512];
	tilecache_deinit(cache);

	/* Get tile directory */
	tile_path(path, 0, 0, -1);

	/* Remove everything inside it */
	rm_rf(path);

	tilecache_init(cache, 0);
}


const uint8_t*
tilecache_get(const Config *conf, TileCache *cache, size_t *len, int x, int y, int zoom)
{
	CURL *new_handle;
	FILE *fd;
	char path[512], url[512];
	uint8_t *png_data;
	struct transfers *ptr;

	/* If download in progress, return null */
	if (!_interrupted) {
		mutex_lock(&_xfers_mutex);
		for (ptr = _xfers; ptr != NULL; ptr = ptr->next) {
			if (ptr->x == x && ptr->y == y && ptr->zoom == zoom) {
				log_debug("%d,%d,%d in progress", x, y, zoom);
				*len = 0;
				mutex_unlock(&_xfers_mutex);
				return NULL;
			}
		}
		mutex_unlock(&_xfers_mutex);
	}

	/* Get path for the current tile */
	tile_path(path, x, y, zoom);

	/* If file exists, return it */
	if ((fd = fopen(path, "rb"))) {
		*len = fread_all(&png_data, fd);
		fclose(fd);
		log_debug("%d,%d,%d found", x, y, zoom);
		return png_data;
	}

	/* File does not exist */
	log_debug("File %s not found, downloading...", path);
	if (!(fd = fopen(path, "wb"))) {
		log_error("Failed to create %s", path);
		*len = 0;
		return NULL;
	}

	/* If worker died, don't attempt to download */
	if (_interrupted) {
		*len = 0;
		return NULL;
	}


	/* Fetch with libcurl */
	endpoint_path(url, conf->tile_base_url, x, y, zoom);
	log_debug("Adding transfer %s", url);
	new_handle = add_download(url, fd);
	if (!new_handle) log_warn("Failed to create CURL handle");
	if (curl_multi_add_handle(cache->curl_handle, new_handle)) {
		log_warn("Failed to add transfer %s", url);

		curl_easy_cleanup(new_handle);
		fclose(fd);
		remove(path);

		*len = 0;
		return NULL;
	}

	/* Add to list of active downloads */
	mutex_lock(&_xfers_mutex);
		ptr = _xfers;
		_xfers = malloc(sizeof(*_xfers));

		_xfers->handle = new_handle;
		_xfers->fd = fd;
		_xfers->next = ptr;
		_xfers->x = x;
		_xfers->y = y;
		_xfers->zoom = zoom;
		_xfers->path = strdup(path);
	mutex_unlock(&_xfers_mutex);
	semaphore_post(&_xfers_sem);


	*len = 0;
	return NULL;
}
/* Static functions {{{ */
static void
endpoint_path(char *buf, const char *base_url, int x, int y, int zoom)
{
	sprintf(buf, "%s/%d/%d/%d.png", base_url, zoom, x, y);
}

static CURL*
add_download(const char *url, FILE *fd)
{
	CURL *handle;

	handle = curl_easy_init();
	curl_easy_setopt(handle, CURLOPT_HEADER, 0);
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, fwrite);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, fd);
	curl_easy_setopt(handle, CURLOPT_USERAGENT, "Sondedump/"VERSION);
	curl_easy_setopt(handle, CURLOPT_URL, url);

	return handle;
}

static void
transfers_deinit(TileCache *cache, struct transfers **_xfers)
{
	struct transfers *ptr, *tmp;

	for (ptr = *_xfers; ptr != NULL;) {
		fclose(ptr->fd);

		log_debug("Removing %s", ptr->path);
		remove(ptr->path);
		free(ptr->path);

		curl_multi_remove_handle(cache->curl_handle, ptr->handle);
		curl_easy_cleanup(ptr->handle);

		tmp = ptr;
		ptr = ptr->next;
		free(tmp);
	}

	_xfers = NULL;
}

static void*
download_thr(void *ptr)
{
	TileCache *cache = (TileCache*)ptr;
	CURLMsg *msg;
	CURL *handle;
	struct transfers *tmp, *prev;
	int alive, msgs_left;

	while (!_interrupted) {
		/* Perform download */
		curl_multi_perform(cache->curl_handle, &alive);

		/* Check which downloads are complete */
		while ((msg = curl_multi_info_read(cache->curl_handle, &msgs_left))) {
			handle = msg->easy_handle;

			/* Search for handle in the transfers list */
			mutex_lock(&_xfers_mutex);
			for (tmp = _xfers, prev = NULL; tmp != NULL; prev = tmp, tmp = tmp->next) {
				/* If handle is found */
				if (tmp->handle == handle) {
					/* Unlink */
					if (prev) prev->next = tmp->next;
					else _xfers = tmp->next;

					/* Close file (or delete it if download failed) */
					if (msg->msg == CURLMSG_DONE) {
						fclose(tmp->fd);
						log_debug("Download complete: %d,%d,%d", tmp->x, tmp->y, tmp->zoom);
					} else {
						remove(tmp->path);
						free(tmp->path);
						log_warn("Download failed");
					}

					free(tmp);
					break;
				}
			}
			mutex_unlock(&_xfers_mutex);

			/* Clean up handle */
			curl_multi_remove_handle(cache->curl_handle, handle);
			curl_easy_cleanup(handle);
			gui_force_update();
		}


		if (alive) {
			curl_multi_wait(cache->curl_handle, NULL, 0, 1000, NULL);
		} else {
			semaphore_wait(&_xfers_sem);
		}
	}

	return NULL;
}

static void
tile_path(char *buf, int x, int y, int zoom)
{
#ifdef _WIN32
	/* Save in %APPDATA%/sondedump.json */
	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, buf))) return NULL;
	strcat(buf, DIR_SEP);
#elif defined(__APPLE__)
	FSRef ref;
	FSFindFolder(kUserDomain, kApplicationSupportFolderType, kCreateFolder, &ref);
	FSRefMakePath(&ref, confpath, LEN(confpath));

	mkdir(buf);
#elif defined(__linux__)
	/* Save in ~/.cache/sondedump/z_x_y.png */
	const char *home = getenv("XDG_CONFIG_HOME");
	if (!home) home = getenv("HOME");
	if (!home) {
		buf[0] = 0;
		return;
	}

	sprintf(buf, "%s/.cache", home);
	mkdir(buf, 0755);
#else
	/* Fallback to cache in current directory */
	confpath[0] = 0;
#endif
	strcat(buf, DIR_SEP "sondedump" DIR_SEP);

#ifdef _WIN32
#elif defined(__APPLE__)
	mkdir(buf);
#elif defined(__linux__)
	mkdir(buf, 0755);
#endif

	if (zoom >= 0) sprintf(buf + strlen(buf), "%d_%d_%d.png", zoom, x, y);
}

/* }}} */
