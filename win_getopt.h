#ifndef win_getopt_h
#define win_getopt_h

extern const char *optarg;
extern int optind;

struct option {
	const char *name;
	int has_arg;
	int *flag;
	int val;
};

int getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts, int *longindex);
#endif
