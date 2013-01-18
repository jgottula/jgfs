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
static const char args_doc[] = "DEVICE";
static struct argp_option options[] = {
	{ 0, 'L', "LABEL", 0,
		"volume label", 0 },
	{ 0, 's', "NUMBER", 0,
		"filesystem size in sectors", 0 },
	{ 0, 'c', "NUMBER", 0,
		"sectors per cluster [must be power of two]", 0 },
	{ 0 }
};
static struct argp argp =
	{ options, &parse_opt, args_doc, doc, NULL, NULL, NULL };

struct {
	char    *dev_path;
	char    *vol_label;
	uint16_t nr_sect;
	uint16_t sect_per_cluster;
} param = { NULL, NULL, 0, 1 };


error_t parse_opt(int key, char *arg, struct argp_state *state) {
	switch (key) {
	case 'L':
		param.vol_label = strdup(arg);
		break;
	case 's':
		switch (sscanf(arg, "%" SCNu16, &param.nr_sect)) {
		case EOF:
			warnx("number of sectors: can't read that!");
			argp_usage(state);
		case 1:
			break;
		default:
			assert(0);
		}
		break;
	case 'c':
		switch (sscanf(arg, "%" SCNu16, &param.sect_per_cluster)) {
		case EOF:
			warnx("sect per cluster: can't read that!");
			argp_usage(state);
		case 1:
			break;
		default:
			assert(0);
		}
		break;
	case ARGP_KEY_ARG:
		if (state->arg_num == 0) {
			param.dev_path = strdup(arg);
		} else {
			warnx("excess argument(s)");
			argp_usage(state);
		}
		break;
	case ARGP_KEY_END:
		if (state->arg_num < 1) {
			warnx("device not specified");
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
 * - reserved area + fat table + nr of clusters must <= fs size
 * write data structures
 * report statistics
 * call sync(2) when done
 */
