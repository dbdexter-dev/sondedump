#ifndef csv_h
#define csv_h

#include <stdio.h>
#include <include/data.h>

typedef struct {
	FILE *fd;
} CSVFile;

int csv_init(CSVFile *file, const char *fname);
void csv_close(CSVFile *file);
void csv_add_point(CSVFile *file, const SondeData *data);

#endif
