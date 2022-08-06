#ifndef sondedump_config_h
#define sondedump_config_h

#define CONFIG_FNAME "sondedump.json"
#define URL_MAXLEN 256
#define TILE_BASE_URL_DEFAULT "https://cartodb-basemaps-c.global.ssl.fastly.net/dark_all/"

typedef struct {
	struct {
		float lat, lon, alt;
	} receiver;

	struct {
		float center_x, center_y, zoom;
	} map;

	struct {
		float temp[4];
		float dewpt[4];
		float rh[4];
		float alt[4];
		float press[4];
		float climb[4];
		float hdg[4];
		float spd[4];
	} colors;

	float ui_scale;
	char tile_base_url[URL_MAXLEN];
} Config;

/**
 * Load configuration from the default file location
 *
 * @param config pointer to config that will be updated
 * @note this function takes care of loading sane configs in case the config
 *       file is corrupted/incomplete/missing
 */
int config_load_from_file(Config *config);

/**
 * Save configuration to the default file location
 *
 * @param config configuration to save
 */
int config_save_to_file(const Config *config);

#endif
