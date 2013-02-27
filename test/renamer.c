/* jgfs
 * (c) 2013 Justin Gottula
 * The source code of this project is distributed under the terms of the
 * simplified BSD license. See the LICENSE file for details.
 */

#include <err.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
	if (mkdir("/mnt/0/dir", 0755) == -1) {
		err(1, "mkdir failed");
	}
	
	if (rename("/mnt/0/dir", "/mnt/0/dir/sub") == -1) {
		err(1, "rename failed");
	}
	
	return 0;
}
