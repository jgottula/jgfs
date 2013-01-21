#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "../common/jgfs.h"
#include "../common/version.h"
#include "../common/macro.h"


#define SECT_PER_CLUSTER 2

#define SZ_TOTAL 2880
#define SZ_RSVD  8
#define SZ_FAT   CEIL(SZ_TOTAL - SZ_RSVD, 256 * SECT_PER_CLUSTER)

#define SZ_NDATA (SZ_RSVD + SZ_FAT)
#define SZ_DATA  (SZ_TOTAL - SZ_NDATA)

#define NUM_CLUSTERS (SZ_DATA / SECT_PER_CLUSTER)

#define DIR_ENT_PER_CLUSTER \
	(((SECT_PER_CLUSTER * 512) / sizeof(jgfs_dir_entry)) - 1)


int dev_fd     = -1;
off_t dev_size = 0;
void *dev_mem  = NULL;

struct jgfs_header      *hdr;
struct jgfs_fat_sector  *fat;
struct jgfs_dir_cluster *root_dir;


void clean_up(void) {
	if (dev_mem != NULL) {
		msync(dev_mem, dev_size, MS_SYNC);
		if (munmap(dev_mem, dev_size) == -1) {
			warn("munmap failed");
		}
		
		dev_mem = NULL;
	}
	
	close(dev_fd);
}

void *get_sector(uint32_t sect_num) {
	if (sect_num >= SZ_TOTAL) {
		errx(1, "get_sector: tried to access past end of device");
	}
	
	return (void *)((char *)dev_mem + (512 * sect_num));
}

void *get_cluster(fat_ent_t fat_ent) {
	if (fat_ent > FAT_LAST) {
		errx(1, "get_cluster: tried to access past FAT_LAST");
	} else if (fat_ent >= NUM_CLUSTERS) {
		errx(1, "get_cluster: tried to access a nonexistent cluster");
	}
	
	/* do NOT call this until hdr has been initialized! */
	return get_sector(hdr->sz_rsvd + hdr->sz_fat +
		(fat_ent * SECT_PER_CLUSTER));
}

void do_header(void) {
	warnx("initializing header");
	
	hdr = get_sector(1);
	
	memcpy(hdr->magic, "JGFS", 4);
	hdr->ver_major = JGFS_VER_MAJOR;
	hdr->ver_minor = JGFS_VER_MINOR;
	
	hdr->sz_total  = SZ_TOTAL;
	hdr->sz_rsvd   = SZ_RSVD;
	hdr->sz_fat    = SZ_FAT;
	
	hdr->s_per_c   = SECT_PER_CLUSTER;
	
	memset(hdr->reserved, 0, sizeof(hdr->reserved));
}

void do_fat(void) {
	warnx("initializing fat");
	
	fat = get_sector(hdr->sz_rsvd);
	
	for (uint16_t i = 0; i < SZ_FAT; ++i) {
		warnx("init fat sector #%" PRIu16, i);
		
		for (uint16_t j = 0; j < 256; ++j) {
			if ((i * 256) + j < NUM_CLUSTERS) {
				fat[i].entries[j] = FAT_FREE;
			} else {
				fat[i].entries[j] = FAT_OOB;
			}
		}
	}
}

void do_root_dir(void) {
	warnx("initializing root dir");
	
	root_dir = get_cluster(FAT_ROOT);
	
	memset(root_dir, 0, SECT_PER_CLUSTER * 512);
	
	root_dir->me     = FAT_ROOT;
	root_dir->parent = FAT_ROOT;
	
	warnx("updating fat");
	fat[0].entries[FAT_ROOT] = FAT_EOF;
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
	
	if ((dev_mem = mmap(NULL, dev_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		dev_fd, 0)) == MAP_FAILED) {
		dev_mem = NULL;
		err(1, "mmap failed");
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
