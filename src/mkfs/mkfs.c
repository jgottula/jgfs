#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "../common/jgfs.h"
#include "../common/version.h"


int dev_fd = -1;
off_t dev_size = 0;


void clean_up(void) {
	close(dev_fd);
}

void do_header(void) {
	struct jgfs_header hdr;
	
	memcpy(hdr.magic, "JGFS", 4);
	hdr.version = JGFS_VERSION;
	
	memset(hdr.label, '\0', sizeof(hdr.label));
	strcpy(hdr.label, "mkjgfs");
	memset(hdr.uuid, 0, sizeof(hdr.uuid));
	
	hdr.sz_c = 0;
	hdr.nr_s = 3360;
	
	hdr.sz_fat = 14;
	
	hdr.fl_dirty = 0;
	
	memset(hdr.reserved, 0, sizeof(hdr.reserved));
	
	
	warnx("writing header");
	
	lseek(dev_fd, 0x0e00, SEEK_SET);
	ssize_t written;
	switch ((written = write(dev_fd, &hdr, sizeof(hdr)))) {
	case -1:
		err(1, "write failed");
	case sizeof(hdr):
		break;
	default:
		errx(1, "incomplete write: %zd/512 bytes", written);
	}
}

int main(int argc, char **argv) {
	warnx("version %s", jgfs_version);
	
	if (argc != 2) {
		errx(1, "expected one argument");
	} else {
		warnx("using device %s", argv[1]);
	}
	
	if ((dev_fd = open(argv[1], O_RDWR)) == -1) {
		err(1, "failed to open %s", argv[1]);
	}
	atexit(&clean_up);
	
	dev_size = lseek(dev_fd, 0, SEEK_END);
	lseek(dev_fd, 0, SEEK_SET);
	
	warnx("%jd bytes / %jd sectors", (intmax_t)dev_size,
		(intmax_t)dev_size / 512);
	if (dev_size % 512 != 0) {
		warnx("device has non-integer number of sectors!");
	}
	
	do_header();
	
	//if (syncfs())
		// warn() on failed sync
	
	warnx("success");
	return 0;
}

/* TODO:
 * use linux (man 2) calls
 * do some sanity checks
 * write data structures
 * report statistics
 * call sync(2) when done
 */
