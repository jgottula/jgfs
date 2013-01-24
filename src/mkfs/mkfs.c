#include <err.h>
#include "../common/jgfs.h"


#define MKFS_S_TOTAL 2880
#define MKFS_S_RSVD  8
#define MKFS_S_PER_C 2


int main(int argc, char **argv) {
	if (argc != 2) {
		errx(1, "expected one argument");
	} else {
		warnx("using device %s", argv[1]);
	}
	
	warnx("hardcoded: %u total sectors", MKFS_S_TOTAL);
	warnx("hardcoded: %u reserved sectors", MKFS_S_RSVD);
	warnx("hardcoded: %u-byte clusters", MKFS_S_PER_C * SECT_SIZE);
	
	jgfs_new(argv[1], MKFS_S_TOTAL, MKFS_S_RSVD, MKFS_S_PER_C);
	jgfs_done();
	
	/* TODO: report some statistics */
	
	warnx("success");
	return 0;
}
