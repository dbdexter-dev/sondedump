#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#define JSMN_HEADER
#include "log/log.h"
#include "libs/cjson/cjson.h"
#include "utils.h"

#ifdef _WIN32
#include <shlobj.h>
#elif defined(__APPLE__)
#include <CoreServices/CoreServices.h>
#include <sys/stat.h>
#elif defined(__linux__)
#include <sys/stat.h>
#endif

#define append_json_array(parent, data_arr, x, len) \
	do {\
		cJSON *_arr = cJSON_CreateArray();\
		for (int i=0; i<len; i++) cJSON_AddItemToArray(_arr, cJSON_CreateNumber(data_arr.x[i]));\
		cJSON_AddItemToObject(parent, #x, _arr);\
	} while(0);


#define MAX_JSON_TOKENS 128
#define CHUNKSIZE 512

static char *config_path(void);
static void parse_map(Config *dst, cJSON *node);
static void parse_receiver(Config *dst, cJSON *node);
static void parse_colors(Config *dst, cJSON *node);
static int parse_number_array(float *dst, cJSON *node, size_t maxlen);

static const Config default_config = {
	.map = {
		.zoom = 3,
	},

	.receiver = {
		.lat = 0,
		.lon = 0,
		.alt = 0
	},

	.colors = {
		.temp  = {224/255.0, 60/255.0, 131/255.0, 1.0},
		.dewpt = {61/255.0, 132/255.0, 224/255.0, 1.0},
		.rh    = {224/255.0, 153/255.0, 60/255.0, 1.0},
		.alt   = {132/255.0, 224/255.0, 61/255.0, 1.0},
		.press = {133/255.0, 44/255.0, 35/255.0, 1.0},
		.climb = {64/255.0, 108/255.0, 30/255.0, 1.0},
		.hdg   = {220/255.0, 102/255.0, 217/255.0, 1.0},
		.spd   = {161/255.0, 101/255.0, 249/255.0, 1.0},
	},

	.ui_scale = 1
};

int
config_load_from_file(Config *config)
{

	cJSON *root, *ptr;
	FILE *fd;
	size_t len;
	char *confdata;

	/* Load sane defaults */
	memcpy(config, &default_config, sizeof(*config));
	config->map.center_x = lon_to_x(0, 0);
	config->map.center_y = lat_to_y(0, 0);

	/* Open config */
	if (!(fd = fopen(config_path(), "r"))) {
		log_warn("Could not find %s, proceeding with defaults", config_path());
		return 1;
	}

	/* Read whole config */
	confdata = NULL;
	len = 0;
	while (!feof(fd)) {
		confdata = realloc(confdata, len + CHUNKSIZE);
		len += fread(confdata + len, 1, CHUNKSIZE, fd);
	}

	log_debug("Read config (size %ld): %s", len, confdata);
	root = cJSON_Parse(confdata);
	free(confdata);

	if (!root) {
		log_warn("Failed to parse config, loading defaults");
		return 1;
	}

	/* Load into config struct */
	for (ptr = root->child; ptr != NULL; ptr = ptr->next) {
		if (ptr->string) {
			if (!strcmp(ptr->string, "map") && cJSON_IsObject(ptr)) {
				parse_map(config, ptr->child);
			} else if (!strcmp(ptr->string, "receiver") && cJSON_IsObject(ptr)) {
				parse_receiver(config, ptr->child);
			} else if (!strcmp(ptr->string, "colors") && cJSON_IsObject(ptr)) {
				parse_colors(config, ptr->child);
			} else if (!strcmp(ptr->string, "ui_scale") && cJSON_IsNumber(ptr)) {
				config->ui_scale = ptr->valuedouble;
			} else {
				log_warn("Unknown field %s (root)", ptr->string);
			}
		}
	}

	log_info("Loaded config from %s", config_path());
	return 0;
}

int
config_save_to_file(const Config *config)
{
	cJSON *root, *receiver, *map, *colors;
	FILE *fd;

	log_info("Saving config...");

	if (!(fd = fopen(config_path(), "w"))) {
		log_error("Failed to open %s for writing", config_path());
		return 1;
	}

	root = cJSON_CreateObject();
	if (!root) {
		log_error("Failed to create json object");
		return 1;
	}

	/* Receiver */
	receiver = cJSON_CreateObject();
	cJSON_AddItemToObject(receiver, "lat", cJSON_CreateNumber(config->receiver.lat));
	cJSON_AddItemToObject(receiver, "lon", cJSON_CreateNumber(config->receiver.lon));
	cJSON_AddItemToObject(receiver, "alt", cJSON_CreateNumber(config->receiver.alt));
	cJSON_AddItemToObject(root, "receiver", receiver);

	/* Map */
	map = cJSON_CreateObject();
	cJSON_AddItemToObject(map, "center_x", cJSON_CreateNumber(config->map.center_x));
	cJSON_AddItemToObject(map, "center_y", cJSON_CreateNumber(config->map.center_y));
	cJSON_AddItemToObject(map, "zoom", cJSON_CreateNumber(config->map.zoom));
	cJSON_AddItemToObject(root, "map", map);

	/* Colors */
	colors = cJSON_CreateObject();
	append_json_array(colors, config->colors, temp, 4);
	append_json_array(colors, config->colors, dewpt, 4);
	append_json_array(colors, config->colors, rh, 4);
	append_json_array(colors, config->colors, alt, 4);
	append_json_array(colors, config->colors, press, 4);
	append_json_array(colors, config->colors, climb, 4);
	append_json_array(colors, config->colors, hdg, 4);
	append_json_array(colors, config->colors, spd, 4);
	cJSON_AddItemToObject(root, "colors", colors);

	/* UI scale */
	cJSON_AddItemToObject(root, "ui_scale", cJSON_CreateNumber(config->ui_scale));


	fprintf(fd, "%s", cJSON_Print(root));
	fclose(fd);

	cJSON_Delete(root);
	log_info("Config saved to %s", config_path());
	return 0;
}

/* Static functions {{{ */
static void
parse_map(Config *dst, cJSON *node)
{
	for (; node != NULL; node = node->next) {
		if (!strcmp(node->string, "center_x") && cJSON_IsNumber(node)) {
			dst->map.center_x = node->valuedouble;
		} else if (!strcmp(node->string, "center_y") && cJSON_IsNumber(node)) {
			dst->map.center_y = node->valuedouble;
		} else if (!strcmp(node->string, "zoom") && cJSON_IsNumber(node)) {
			dst->map.zoom = node->valuedouble;
		} else {
			log_warn("Unknown field %s", node->string);
		}
	}
}

static void
parse_receiver(Config *dst, cJSON *node)
{
	for (; node != NULL; node = node->next) {
		if (!strcmp(node->string, "lat") && cJSON_IsNumber(node)) {
			dst->receiver.lat = node->valuedouble;
		} else if (!strcmp(node->string, "lon") && cJSON_IsNumber(node)) {
			dst->receiver.lon = node->valuedouble;
		} else if (!strcmp(node->string, "alt") && cJSON_IsNumber(node)) {
			dst->receiver.alt = node->valuedouble;
		} else {
			log_warn("Unknown field %s", node->string);
		}
	}
}

static void
parse_colors(Config *config, cJSON *node)
{
	for (; node != NULL; node = node->next) {
		if (!strcmp(node->string, "temp") && cJSON_IsArray(node)) {
			parse_number_array(config->colors.temp, node->child, LEN(config->colors.temp));
		} else if (!strcmp(node->string, "dewpt") && cJSON_IsArray(node)) {
			parse_number_array(config->colors.dewpt, node->child, LEN(config->colors.dewpt));
		} else if (!strcmp(node->string, "rh") && cJSON_IsArray(node)) {
			parse_number_array(config->colors.rh, node->child, LEN(config->colors.rh));
		} else if (!strcmp(node->string, "alt") && cJSON_IsArray(node)) {
			parse_number_array(config->colors.alt, node->child, LEN(config->colors.alt));
		} else if (!strcmp(node->string, "press") && cJSON_IsArray(node)) {
			parse_number_array(config->colors.press, node->child, LEN(config->colors.press));
		} else if (!strcmp(node->string, "climb") && cJSON_IsArray(node)) {
			parse_number_array(config->colors.climb, node->child, LEN(config->colors.climb));
		} else if (!strcmp(node->string, "hdg") && cJSON_IsArray(node)) {
			parse_number_array(config->colors.hdg, node->child, LEN(config->colors.hdg));
		} else if (!strcmp(node->string, "spd") && cJSON_IsArray(node)) {
			parse_number_array(config->colors.spd, node->child, LEN(config->colors.spd));
		} else {
			log_warn("Unknown field %s", node->string);
		}
	}
}

static int
parse_number_array(float *dst, cJSON *node, size_t maxlen)
{
	for (; node != NULL && maxlen > 0; maxlen--, node = node->next) {
		if (!cJSON_IsNumber(node)) return 1;
		*dst++ = node->valuedouble;
	}

	return 0;
}

/**
 * Get the OS-specific default configuration file path
 *
 * @return the absolute path where the config should be written. This path is
 * guaranteed to exist (bar the actual file)
 */
static char*
config_path(void)
{
	static char confpath[512];
#ifdef _WIN32
	/* Save in %APPDATA%/sondedump.json */
	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, confpath))) return NULL;
	strcat(confpath, "\\");
#elif defined(__APPLE__)
	FSRef ref;
	FSFindFolder(kUserDomain, kApplicationSupportFolderType, kCreateFolder, &ref);
	FSRefMakePath(&ref, confpath, LEN(confpath));

	mkdir(confpath);
#elif defined(__linux__)
	/* Save in ~/.config/sondedump.json */
	const char *home = getenv("XDG_CONFIG_HOME");
	if (!home) home = getenv("HOME");
	if (!home) return NULL;

	sprintf(confpath, "%s/.config/", home);
	mkdir(confpath, 0755);

#else
	/* Fallback to config in current directory */
	confpath[0] = 0;
#endif
	strcat(confpath, CONFIG_FNAME);
	return confpath;
}
/* }}} */
