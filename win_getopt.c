#include <string.h>
#include "win_getopt.h"

const char *optarg;
int optind = 1;

int
getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts, int *longindex)
{
	const char *cur_argv;
	const struct option *cur_option;
	int opt_len;
	int i;

	for (; optind < argc; optind++) {
		cur_argv = argv[optind];

		if (cur_argv[1] == '-') {
			/* Long option */
			for(i=0, cur_option = longopts; cur_option->name != NULL; cur_option++, i++) {
				opt_len = strlen(cur_option->name);
				if (!strncmp(cur_argv+2, cur_option->name, opt_len)) {
					if (cur_option->has_arg) optarg = &cur_argv[2 + opt_len];
					if (longindex) *longindex = i;
					optind++;
					return cur_option->val;
				}
			}
		} else {
			/* Short option */
			for(i=0, cur_option = longopts; cur_option->name != NULL; cur_option++, i++) {
				if (cur_argv[2] == cur_option->val) {
					if (cur_option->has_arg) optarg = &cur_argv[4];
					if (longindex) *longindex = i;
					optind++;
					return cur_option->val;
				}
			}
		}
	}

	return -1;
}
