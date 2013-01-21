#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION  26
#include <err.h>
#include <fuse.h>
#include <string.h>


extern char *dev_path;

extern struct fuse_operations jg_oper;


int main(int argc, char **argv) {
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
	
	return fuse_main(real_argc, real_argv, &jg_oper, NULL);
}
