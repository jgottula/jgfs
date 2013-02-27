/* jgfs
 * (c) 2013 Justin Gottula
 * The source code of this project is distributed under the terms of the
 * simplified BSD license. See the LICENSE file for details. */


#include <err.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
	int fd;
	
	if ((fd = open(argv[1], O_RDONLY)) == -1) {
		err(1, "open failed");
	}
	
	if (syncfs(fd) == -1) {
		err(1, "syncfs failed");
	}
	
	close(fd);
	
	return 0;
}
