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


#define SZ_TOTAL 2880
#define SZ_RSVD  8
#define SZ_FAT   12

#define SZ_NDATA (SZ_RSVD + SZ_FAT)
#define SZ_DATA  (SZ_TOTAL - SZ_NDATA)


int dev_fd     = -1;
off_t dev_size = 0;

struct jgfs_header      hdr;
struct jgfs_fat_sector *fat;
struct jgfs_dir_cluster root_dir;


void clean_up(void) {
	close(dev_fd);
}

void write_sector(uint16_t sect, const void *data) {
	ssize_t b_written;
	
	lseek(dev_fd, sect * 0x200, SEEK_SET);
	
	switch ((b_written = write(dev_fd, data, 512))) {
	case -1:
		err(1, "write_sector #%" PRIu16 " failed", sect);
	case 512:
		break;
	default:
		errx(1, "write_sector #%" PRIu16 " incomplete: %zd/512 bytes",
			sect, b_written);
	}
}

void do_header(void) {
	memcpy(hdr.magic, "JGFS", 4);
	hdr.ver_major = JGFS_VER_MAJOR;
	hdr.ver_minor = JGFS_VER_MINOR;
	
	hdr.sz_total  = SZ_TOTAL;
	hdr.sz_rsvd   = SZ_RSVD;
	hdr.sz_fat    = SZ_FAT;
	
	memset(hdr.reserved, 0, sizeof(hdr.reserved));
	
	warnx("writing header");
	write_sector(1, &hdr);
}

void do_fat(void) {
	fat = malloc(SZ_FAT * sizeof(struct jgfs_fat_sector));
	
	memset(fat, FAT_OOB, SZ_FAT * sizeof(struct jgfs_fat_sector));
	
	/* root directory cluster */
	fat[0].entries[0] = FAT_EOF;
	
	for (uint16_t s = 1; s < SZ_DATA; ++s) {
		uint16_t fat_sect = s / 256;
		uint16_t fat_idx  = s % 256;
		
		fat[fat_sect].entries[fat_idx] = FAT_FREE;
	}
	
	for (uint16_t i = 0; i < SZ_FAT; ++i) {
		warnx("writing fat sector #%" PRIu16, i);
		write_sector(SZ_RSVD + i, fat + i);
	}
}

void do_root_dir(void) {
	root_dir.me     = 0;
	root_dir.parent = 0;
	
	memset(root_dir.reserved, 0, sizeof(root_dir.reserved));
	memset(root_dir.entries, 0, sizeof(root_dir.entries));
	
	strcpy(root_dir.entries[0].name, "dir");
	root_dir.entries[0].attrib = ATTR_DIR;
	root_dir.entries[0].begin  = 1;
	root_dir.entries[0].size   = 512;
	
	strcpy(root_dir.entries[1].name, "file1");
	root_dir.entries[1].attrib = ATTR_FILE;
	root_dir.entries[1].begin = 2;
	root_dir.entries[1].size  = 42;
	
	strcpy(root_dir.entries[2].name, "link_to_file2");
	root_dir.entries[2].attrib = ATTR_SYMLINK;
	root_dir.entries[2].begin  = 3;
	root_dir.entries[2].size   = 9;
	
	warnx("writing root directory");
	write_sector(SZ_NDATA, &root_dir);
	
	
	struct jgfs_dir_cluster sub_dir;
	
	sub_dir.me     = 1;
	sub_dir.parent = 0;
	
	memset(sub_dir.reserved, 0, sizeof(sub_dir.reserved));
	memset(sub_dir.entries, 0, sizeof(sub_dir.entries));
	
	strcpy(sub_dir.entries[0].name, "file2");
	sub_dir.entries[0].attrib = ATTR_FILE;
	sub_dir.entries[0].begin = 4;
	sub_dir.entries[0].size = 1024;
	
	strcpy(sub_dir.entries[1].name, "empty_dir");
	sub_dir.entries[1].attrib = ATTR_DIR;
	sub_dir.entries[1].begin = 6;
	sub_dir.entries[1].size = 512;
	
	warnx("writing second directory cluster");
	write_sector(SZ_NDATA + 1, &sub_dir);
	
	
	struct jgfs_dir_cluster empty_dir;
	
	empty_dir.me     = 6;
	empty_dir.parent = 1;
	
	memset(empty_dir.reserved, 0, sizeof(empty_dir.reserved));
	memset(empty_dir.entries, 0, sizeof(empty_dir.entries));
	
	warnx("writing third directory cluster");
	write_sector(SZ_NDATA + 6, &empty_dir);
	
	
	uint8_t buffer[512];
	
	memset(buffer, 0, sizeof(buffer));
	strcpy((char *)buffer, "*************\nthis is file1\n*************\n");
	
	warnx("writing file1 data");
	write_sector(SZ_NDATA + 2, buffer);
	
	
	memset(buffer, 0, sizeof(buffer));
	strcpy((char *)buffer, "dir/file2");
	
	warnx("writing link_to_file2 path");
	write_sector(SZ_NDATA + 3, buffer);
	
	
	memset(buffer, 0x04, sizeof(buffer));
	
	warnx("writing file2 data [1/2]");
	write_sector(SZ_NDATA + 4, buffer);
	
	
	memset(buffer, 0x05, sizeof(buffer));
	
	warnx("writing file2 data [2/2]");
	write_sector(SZ_NDATA + 5, buffer);
	
	
	/* rewrite the first fat sector so these clusters show up as used */
	fat[0].entries[1] = FAT_EOF; // dir
	fat[0].entries[2] = FAT_EOF; // file1
	fat[0].entries[3] = FAT_EOF; // link_to_file2
	fat[0].entries[4] = 5;       // file2
	fat[0].entries[5] = FAT_EOF; // file2
	fat[0].entries[6] = FAT_EOF; // empty_dir
	
	warnx("rewriting sector #0 of the fat");
	write_sector(SZ_RSVD, fat);
}

int main(int argc, char **argv) {
	warnx("version 0x%02x%02x", JGFS_VER_MAJOR, JGFS_VER_MINOR);
	
	if (argc != 2) {
		errx(1, "expected one argument");
	} else {
		warnx("using device %s", argv[1]);
	}
	
	if ((dev_fd = open(argv[1], O_RDWR)) == -1) {
		err(1, "failed to open %s", argv[1]);
	}
	atexit(clean_up);
	
	dev_size = lseek(dev_fd, 0, SEEK_END);
	lseek(dev_fd, 0, SEEK_SET);
	
	warnx("%jd bytes / %jd sectors", (intmax_t)dev_size,
		(intmax_t)dev_size / 512);
	if (dev_size % 512 != 0) {
		warnx("device has non-integer number of sectors!");
	}
	
	do_header();
	do_fat();
	do_root_dir();
	
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
