#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#define JSMN_HEADER
#include "log/log.h"
#include "libs/jsmn/jsmn.h"
#include "utils.h"

#ifdef _WIN32
#include <shlobj.h>
#elif defined(__APPLE__)
#include <CoreServices/CoreServices.h>
#include <sys/stat.h>
#elif defined(__linux__)
#include <sys/stat.h>
#endif

#define MAX_JSON_TOKENS 128
#define CHUNKSIZE 512

static int json_cmp(const char *json, jsmntok_t *token, const char *s);
static char *config_path(void);

static const char *config_keys_str[] = {
	"ground_lat",
	"ground_lon",
	"ground_alt",
	"ui_scale",
	"map_x",
	"map_y",
	"map_zoom"
};

enum config_keys {
	GROUND_LAT=0,
	GROUND_LON=1,
	GROUND_ALT=2,
	UI_SCALE=3,
	MAP_X=4,
	MAP_Y=5,
	MAP_ZOOM=6,
};

int
config_load_from_file(Config *config)
{
	FILE *fd;
	size_t len;
	char *confdata;

	jsmn_parser parser;
	jsmntok_t tokens[MAX_JSON_TOKENS];
	int i, j, token_count;

	/* Load sane defaults */
	config->receiver.lat = 0;
	config->receiver.lon = 0;
	config->receiver.alt = 0;
	config->ui_scale = 1.0;
	config->map.zoom = 3.0;
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
		len += fread(confdata, 1, CHUNKSIZE, fd);
	}

	log_debug("Read config (size %ld): %s", len, confdata);

	/* Parse */
	jsmn_init(&parser);
	token_count = jsmn_parse(&parser, confdata, len, tokens, MAX_JSON_TOKENS);
	if (token_count < 0) {
		log_error("Invalid configuration file (jsmn error code %d)", token_count);
		return 2;
	}

	if (tokens[0].type != JSMN_OBJECT) {
		log_error("Invalid configuration file (top-level object not found)");
		return 3;
	}


	/* Load into config struct */
	for (i=0; i<token_count; i++) {
		for (j=0; j<(int)LEN(config_keys_str); j++) {
			if (!json_cmp(confdata, &tokens[i], config_keys_str[j])) {
				switch(j) {
				case GROUND_LAT:
					sscanf(confdata + tokens[i+1].start, "%f", &config->receiver.lat);
					break;
				case GROUND_LON:
					sscanf(confdata + tokens[i+1].start, "%f", &config->receiver.lon);
					break;
				case GROUND_ALT:
					sscanf(confdata + tokens[i+1].start, "%f", &config->receiver.alt);
					break;
				case UI_SCALE:
					sscanf(confdata + tokens[i+1].start, "%f", &config->ui_scale);
					break;
				case MAP_X:
					sscanf(confdata + tokens[i+1].start, "%f", &config->map.center_x);
					break;
				case MAP_Y:
					sscanf(confdata + tokens[i+1].start, "%f", &config->map.center_y);
					break;
				case MAP_ZOOM:
					sscanf(confdata + tokens[i+1].start, "%f", &config->map.zoom);
					break;
				default:
					log_warn("Unknown config key: %s", confdata + tokens[i].start);
					break;
				}
			}
		}
	}
	free(confdata);

	log_info("Loaded config from %s", config_path());

	return 0;
}

int
config_save_to_file(const Config *config)
{
	FILE *fd;

	log_info("Saving to %s...", config_path());

	if (!(fd = fopen(config_path(), "w"))) {
		log_error("Failed to write %s\n", config_path());
		return 1;
	}

	fprintf(fd, "{\n");
	fprintf(fd, "\t\"%s\": %f,\n", config_keys_str[GROUND_LAT], config->receiver.lat);
	fprintf(fd, "\t\"%s\": %f,\n", config_keys_str[GROUND_LON], config->receiver.lon);
	fprintf(fd, "\t\"%s\": %f,\n", config_keys_str[GROUND_ALT], config->receiver.alt);
	fprintf(fd, "\t\"%s\": %f,\n", config_keys_str[UI_SCALE], config->ui_scale);
	fprintf(fd, "\t\"%s\": %f,\n", config_keys_str[MAP_X], config->map.center_x);
	fprintf(fd, "\t\"%s\": %f,\n", config_keys_str[MAP_Y], config->map.center_y);
	fprintf(fd, "\t\"%s\": %f\n", config_keys_str[MAP_ZOOM], config->map.zoom);
	fprintf(fd, "}\n");

	fclose(fd);
	return 0;
}

/* Static functions {{{ */
/**
 * Compare a string with the tokenized value returned by jsmn
 *
 * @param json  full json string passed to jsmn
 * @param token token returned by jsmn, hopefully pointing to a valid string
 * @param s     string to compare to the token
 * @return      0 on match, non-zero on mismatch or invalid jsmn token type
 */
static int
json_cmp(const char *json, jsmntok_t *token, const char *s)
{
	if (token->type == JSMN_STRING && (int)strlen(s) == token->end - token->start) {
		return strncmp(json + token->start, s, token->end - token->start);
	}
	return 1;
}

/**
 * Get the OS-specific default configuration file path
 *
 * @return the absolute path where the config should be written. This path is
 * guaranteed to exist (bar the actual file)
 * FIXME heavily untested outside of __linux__
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
