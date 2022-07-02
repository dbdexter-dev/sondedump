#ifndef config_h
#define config_h

#define CONFIG_FNAME "sondedump.json"

typedef struct {
	struct {
		float lat, lon, alt;
	} receiver;
	float ui_scale;
} Config;

int config_load_from_file(Config *config);
int config_save_to_file(const Config *config);

#endif
