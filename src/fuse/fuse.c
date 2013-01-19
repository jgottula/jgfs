#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION  26
#include <err.h>
#include <fuse.h>
#include <string.h>
#include "fs.h"
#include "../common/version.h"


char *dev_path;


static struct fuse_operations jgfs_oper = {
	.getattr = jgfs_getattr,
	.readdir = jgfs_readdir,
	.init    = jgfs_init,
	.destroy = jgfs_destroy,
};


int main(int argc, char **argv) {
	
	warnx("version 0x%02x%02x", JGFS_VER_MAJOR, JGFS_VER_MINOR);
	
	if (argc != 3) {
		errx(1, "expected two arguments");
	}
	
	dev_path = argv[1];
	
	/* fuse's command processing is inflexible and useless */
	int real_argc = 4;
	char **real_argv = malloc(real_argc * sizeof(char *));
	real_argv[0] = argv[0];
	real_argv[1] = strdup("-s");
	real_argv[2] = strdup("-d");
	real_argv[3] = argv[2];
	
	return fuse_main(real_argc, real_argv, &jgfs_oper, NULL);
}
