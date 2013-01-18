#define _GNU_SOURCE

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


void usage(void) {
	/* this uses a GNU extension to get the program name, but so does errx */
	errx(1, "usage: %s <device>", program_invocation_short_name);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		warnx("expected one argument");
		usage();
	}
	
	return 0;
}

/* TODO:
 * use linux (man 2) calls
 * do some sanity checks
 * write data structures
 * report statistics
 * call sync(2) when done
 */
