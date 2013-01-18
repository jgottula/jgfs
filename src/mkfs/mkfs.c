#include <argp.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/jgfs.h"
#include "../common/version.h"


/* argp structures */
error_t parse_opt(int, char *, struct argp_state *);
static const char doc[] = "Initialize jgfs on a block device.";
static const char args_doc[] = "DEVICE LABEL";
static struct argp_option options[] = {
	{ 0, 'c', "NUMBER", 0,
		"sectors per cluster [must be power of two]", 0 },
	{ 0, 's', "SIZE", 0,
		"filesystem size [suffixes: s c K M]", 0 },
	{ 0 }
};
static struct argp argp =
	{ options, &parse_opt, args_doc, doc, NULL, NULL, NULL };

struct {
	char    *dev_path;
	char    *vol_label;
	uint8_t  sect_per_cluster;
	
	struct {
		uint32_t quantity;
		enum { SUFFIX_NONE, SUFFIX_SECT, SUFFIX_CLUSTER, SUFFIX_K, SUFFIX_M }
			suffix;
	} fs_size;
} param = { NULL, NULL, 1, { 0, SUFFIX_NONE } };


error_t parse_opt(int key, char *arg, struct argp_state *state) {
	switch (key) {
	case 'c':
		switch (sscanf(arg, "%" SCNu8,
			&param.sect_per_cluster)) {
		case EOF:
			warnx("sect per cluster: can't read that!");
			argp_usage(state);
		case 1:
			break;
		default:
			assert(0);
		}
		break;
	case 's':
	{
		char suffix;
		switch (sscanf(arg, "%" SCNu32 "%c",
			&param.fs_size.quantity, &suffix)) {
		case EOF:
			warnx("fs size: can't read that!");
			argp_usage(state);
		case 2:
			if (suffix == 's') {
				param.fs_size.suffix = SUFFIX_SECT;
			} else if (suffix == 'c') {
				param.fs_size.suffix = SUFFIX_CLUSTER;
			} else if (suffix == 'K') {
				param.fs_size.suffix = SUFFIX_K;
			} else if (suffix == 'M') {
				param.fs_size.suffix = SUFFIX_M;
			} else {
				warnx("fs size: bad suffix!");
				argp_usage(state);
			}
		case 1:
			break;
		default:
			assert(0);
		}
	}
		break;
	case ARGP_KEY_ARG:
		if (state->arg_num == 0) {
			param.dev_path = strdup(arg);
		} else if (state->arg_num == 1) {
			param.vol_label = strdup(arg);
		} else {
			warnx("excess argument(s)");
			argp_usage(state);
		}
		break;
	case ARGP_KEY_END:
		if (state->arg_num < 1) {
			warnx("device not specified");
			argp_usage(state);
		} else if (state->arg_num < 2) {
			warnx("label not specified");
			argp_usage(state);
		}
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	
	return 0;
}

int main(int argc, char **argv) {
	/* how to get device? */
	argp_parse(&argp, argc, argv, 0, NULL, NULL);
	
	return 0;
}

/* TODO:
 * use linux (man 2) calls
 * do some sanity checks:
 * - fs size must not exceed device size
 * - fs size must be above a certain minimum
 * - fs size must be an integer number of clusters
 * - fs size in clusters must not exceed entry size in bits
 * write data structures
 * report statistics
 * call sync(2) when done
 */
