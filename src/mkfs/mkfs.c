#include <argp.h>
#include <bsd/string.h>
#include <err.h>
#include <inttypes.h>
#include "../common/jgfs.h"


/* configurable parameters
 * NOTE: be sure to update argp documentation when changing these default */
const char *dev_path = NULL;
struct jgfs_mkfs_param param = {
	.label = "mkjgfs",
	
	.s_total = 0, // auto
	.s_boot  = 8,
	.s_per_c = 0, // auto
	
	.zero_data = false,
	.zap       = false,
};


error_t parse_opt(int key, char *arg, struct argp_state *state) {
	switch (key) {
	case 'L':
		if (strlen(arg) > JGFS_LABEL_LIMIT) {
			errx(1, "labels cannot be longer than %u characters",
				JGFS_LABEL_LIMIT);
		}
		
		strlcpy(param.label, arg, JGFS_LABEL_LIMIT + 1);
		break;
	case 's':
		switch (sscanf(arg, "%" SCNu32, &param.s_total)) {
		case EOF:
			warnx("s_total: can't read that!");
			argp_usage(state);
		case 1:
			break;
		}
		break;
	case 'b':
		switch (sscanf(arg, "%" SCNu16, &param.s_boot)) {
		case EOF:
			warnx("s_boot: can't read that!");
			argp_usage(state);
		case 1:
			break;
		}
		break;
	case 'c':
		switch (sscanf(arg, "%" SCNu16, &param.s_per_c)) {
		case EOF:
			warnx("s_per_c: can't read that!");
			argp_usage(state);
		case 1:
			break;
		}
		break;
	case 'z':
		param.zero_data = true;
		break;
	case 'Z':
		param.zap = true;
		break;
	case ARGP_KEY_ARG:
		if (state->arg_num == 0) {
			dev_path = strdup(arg);
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


/* argp structures */
const char *argp_program_version = "jgfs " STRIFY(JGFS_VER_TOTAL);
static const char doc[] = "Initialize jgfs on a block device.";
static const char args_doc[] = "DEVICE";
static struct argp_option options[] = {
	{ NULL, 0, NULL, 0, "string parameters:", 1 },
	{ "label", 'L', "LABEL", 0,
		"filesystem label        [default: mkjgfs]", 1 },
	
	{ NULL, 0, NULL, 0, "size parameters:", 2 },
	{ "size", 's', "NUMBER", 0,
		"total sectors           [default: auto]", 2, },
	{ "boot", 'b', "NUMBER", 0,
		"boot sectors            [default: 8]", 2, },
	{ "cluster", 'c', "NUMBER", 0,
		"sectors per cluster     [default: auto]", 2, },
	
	{ NULL, 0, NULL, 0, "initialization options:", 3 },
	{ "zero-data", 'z', NULL, 0,
		"zero out data clusters  [default: no]", 3 },
	{ "zap", 'Z', NULL, 0,
		"zap vbr and boot area   [default: no]", 3 },
	
	{ 0 }
};
static struct argp argp =
	{ options, &parse_opt, args_doc, doc, NULL, NULL, NULL };


int main(int argc, char **argv) {
	argp_parse(&argp, argc, argv, 0, NULL, NULL);
	
	jgfs_new(dev_path, &param);
	
	warnx("syncing filesystem");
	jgfs_sync();
	
	/* report some statistics */
	warnx("total sectors:  %" PRIu32, jgfs.hdr->s_total);
	warnx("boot sectors:   %" PRIu16, jgfs.hdr->s_boot);
	warnx("fat sectors:    %" PRIu16, jgfs.hdr->s_fat);
	warnx("cluster size:   %u", jgfs.hdr->s_per_c * SECT_SIZE);
	warnx("total clusters: %" PRIu16, jgfs_fs_clusters());
	
	jgfs_done();
	
	warnx("success");
	return 0;
}
