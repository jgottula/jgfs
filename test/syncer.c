#include <err.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
	int fd;
	
	if ((fd = open("/mnt/0/file", O_RDONLY)) != 0) {
		err(1, "open failed");
	}
	
	if (syncfs(fd) != 0) {
		err(1, "syncfs failed");
	}
	
	close(fd);
	
	return 0;
}
