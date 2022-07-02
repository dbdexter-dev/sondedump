#ifndef config_h
#define config_h

#define CONFIG_FNAME "sondedump.json"

typedef struct {
	struct {
		float lat, lon, alt;
	} receiver;
	float ui_scale;
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
