#include "csv.h"

#define CSV_TIME_FORMAT "%Y-%m-%dT%H:%M:%SZ"

int
csv_init(CSVFile *file, const char *fname)
{
	file->fd = fopen(fname, "wb");

	if (!file->fd) return 1;

	fprintf(file->fd, "Time,Temperature,RH,Pressure,Latitude,Longitude,Altitude,Speed,Heading,Climb,XDATA\n");

	return 0;
}

void
csv_close(CSVFile *file)
{
	if (file && file->fd) {
		fclose(file->fd);
		file->fd = NULL;
	}
}

void
csv_add_point(CSVFile *file, const SondeData *data)
{
	char timestr[sizeof("YYYY-MM-DDThh:mm:ssZ")+1];

	if (data->fields & DATA_TIME) {
		strftime(timestr, sizeof(timestr), CSV_TIME_FORMAT, gmtime(&data->time));
		fprintf(file->fd, "%s,", timestr);
	} else {
		fprintf(file->fd, ",");
	}


	if (data->fields & DATA_PTU) {
		fprintf(file->fd, "%f,%f,%f,", data->temp, data->rh, data->pressure);
	} else {
		fprintf(file->fd, ",,,");
	}

	if (data->fields & DATA_POS) {
		fprintf(file->fd, "%f,%f,%f,", data->lat, data->lon, data->alt);
	} else {
		fprintf(file->fd, ",,,");
	}

	if (data->fields & DATA_SPEED) {
		fprintf(file->fd, "%f,%f,%f,", data->speed, data->heading, data->climb);
	} else {
		fprintf(file->fd, ",,,");
	}

	if (data->fields & DATA_XDATA) {
		fprintf(file->fd, "O3=%fppb", data->xdata.o3_ppb);
	} else {
	}

	fprintf(file->fd, "\n");
}
