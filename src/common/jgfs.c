#include "jgfs.h"
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>


static int      dev_fd   = -1;
static void    *dev_mem  = NULL;
static uint64_t dev_size = 0;
static uint64_t dev_sect = 0;

struct jgfs_hdr      *hdr  = NULL;
struct sect          *rsvd = NULL;
struct jgfs_fat_sect *fat  = NULL;

static uint16_t clusters_total = 0;


static void jgfs_msync(void) {
	if (msync(dev_mem, dev_size, MS_SYNC) == -1) {
		warn("msync failed");
	}
}

static void jgfs_fsync(void) {
	if (fsync(dev_fd) == -1) {
		warn("fsync failed");
	}
}

static void jgfs_clean_up(void) {
	if (dev_mem != NULL) {
		jgfs_msync();
		
		if (munmap(dev_mem, dev_size) == -1) {
			warn("munmap failed");
		}
		dev_mem = NULL;
	}
	
	if (dev_fd != -1) {
		jgfs_fsync();
		
		if (close(dev_fd) == -1) {
			warn("close failed");
		}
		dev_fd = -1;
	}
}

static void jgfs_init_real(const char *dev_path,
	const struct jgfs_hdr *new_hdr) {
	warnx("using jgfs version 0x%02x%02x", JGFS_VER_MAJOR, JGFS_VER_MINOR);
	
	atexit(jgfs_clean_up);
	
	if ((dev_fd = open(dev_path, O_RDWR)) == -1) {
		err(1, "failed to open '%s'", dev_path);
	}
	
	dev_size = lseek(dev_fd, 0, SEEK_END);
	lseek(dev_fd, 0, SEEK_SET);
	
	dev_sect = dev_size / SECT_SIZE;
	
	if (dev_sect < 2) {
		errx(1, "device has less than two sectors");
	} else if (dev_size % 512 != 0) {
		warnx("device has non-integer number of sectors");
	}
	
	if ((dev_mem = mmap(NULL, dev_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		dev_fd, 0)) == MAP_FAILED) {
		err(1, "mmap failed");
	}
	
	hdr = jgfs_get_sect(JGFS_HDR_SECT);
	
	if (new_hdr != NULL) {
		memset(hdr, 0, SECT_SIZE);
		memcpy(hdr, new_hdr, sizeof(*hdr));
	}
	
	if (memcmp(JGFS_MAGIC, hdr->magic, sizeof(hdr->magic)) != 0) {
		errx(1, "jgfs header not found");
	}
	
	if (hdr->ver_major != JGFS_VER_MAJOR || hdr->ver_minor != JGFS_VER_MINOR) {
		errx(1, "incompatible filesystem (%#06" PRIx16 ")",
			JGFS_VER_EXPAND(hdr->ver_major, hdr->ver_minor));
	}
	
	if (dev_sect < hdr->s_total) {
		errx(1, "filesystem exceeds device bounds");
	}
	
	rsvd = jgfs_get_sect(JGFS_HDR_SECT + 1);
	fat  = jgfs_get_sect(hdr->s_rsvd);
	
	clusters_total = (hdr->s_total - (hdr->s_rsvd + hdr->s_fat)) / hdr->s_per_c;
	
	if (hdr->s_fat < CEIL(clusters_total, JGFS_FENT_PER_S)) {
		errx(1, "fat is too small");
	}
}

void jgfs_init(const char *dev_path) {
	jgfs_init_real(dev_path, NULL);
}

void jgfs_new(const char *dev_path,
	uint32_t s_total, uint16_t s_rsvd, uint16_t s_per_c) {
	struct jgfs_hdr new_hdr;
	
	memset(&new_hdr, 0, sizeof(new_hdr));
	
	memcpy(new_hdr.magic, JGFS_MAGIC, sizeof(new_hdr.magic));
	new_hdr.ver_major = JGFS_VER_MAJOR;
	new_hdr.ver_minor = JGFS_VER_MINOR;
	
	new_hdr.s_total = s_total;
	new_hdr.s_rsvd  = s_rsvd;
	
	new_hdr.s_per_c = s_per_c;
	
	/* iteratively calculate optimal fat size, taking into account the size of
	 * the fat itself when determining the number of available clusters */
	uint16_t s_fat = 1;
	do {
		new_hdr.s_fat = s_fat;
		s_fat = CEIL(new_hdr.s_total - (new_hdr.s_rsvd + new_hdr.s_fat),
			JGFS_FENT_PER_S);
	} while (new_hdr.s_fat != s_fat);
	
	new_hdr.root_dir_ent.type  = TYPE_DIR;
	new_hdr.root_dir_ent.mtime = time(NULL);
	new_hdr.root_dir_ent.size  = jgfs_clust_size();
	new_hdr.root_dir_ent.begin = FAT_ROOT;
	
	jgfs_init_real(dev_path, &new_hdr);
	
	/* initialize the root directory cluster */
	struct jgfs_dir_clust *root_dir_clust = jgfs_get_clust(FAT_ROOT);
	memset(root_dir_clust, 0, jgfs_clust_size());
	root_dir_clust->me     = FAT_ROOT;
	root_dir_clust->parent = FAT_ROOT;
	
	jgfs_fat_write(FAT_ROOT, FAT_EOF);
}

void jgfs_done(void) {
	jgfs_clean_up();
}

void jgfs_sync(void) {
	jgfs_msync();
	jgfs_fsync();
}

uint32_t jgfs_clust_size(void) {
	return (SECT_SIZE * hdr->s_per_c);
}

void *jgfs_get_sect(uint32_t sect_num) {
	if (sect_num >= dev_sect) {
		errx(1, "jgfs_get_sect: tried to access past end of device "
			"(sect %" PRIu32 ")", sect_num);
	}
	
	return (void *)((struct sect *)dev_mem + sect_num);
}

void *jgfs_get_clust(fat_ent_t clust_num) {
	if (clust_num > FAT_LAST) {
		errx(1, "jgfs_get_clust: tried to access past FAT_LAST "
			"(clust %#06" PRIx16 ")", clust_num);
	} else if (clust_num >= clusters_total) {
		errx(1, "jgfs_get_clust: tried to access nonexistent cluster "
			"(clust %#06" PRIx16 ")", clust_num);
	}
	
	return jgfs_get_sect(hdr->s_rsvd + hdr->s_fat + (clust_num * hdr->s_per_c));
}

fat_ent_t jgfs_fat_read(fat_ent_t addr) {
	uint16_t fat_sect = addr / JGFS_FENT_PER_S;
	uint16_t fat_idx  = addr % JGFS_FENT_PER_S;
	
	if (fat_sect >= hdr->s_fat) {
		errx(1, "jgfs_fat_read: tried to access past s_fat "
			"(fat %#06" PRIx16 ")", addr);
	}
	
	return fat[fat_sect].entries[fat_idx];
}

void jgfs_fat_write(fat_ent_t addr, fat_ent_t val) {
	uint16_t fat_sect = addr / JGFS_FENT_PER_S;
	uint16_t fat_idx  = addr % JGFS_FENT_PER_S;
	
	if (fat_sect >= hdr->s_fat) {
		errx(1, "jgfs_fat_write: tried to access past s_fat "
			"(fat %#06" PRIx16 ")", addr);
	}
	
	fat[fat_sect].entries[fat_idx] = val;
}

bool jgfs_find_free_clust(fat_ent_t *dest) {
	for (uint16_t i = 0; i < hdr->s_fat; ++i) {
		for (uint16_t j = 0; j < JGFS_FENT_PER_S; ++j) {
			if (fat[i].entries[j] == FAT_FREE) {
				*dest = (i * JGFS_FENT_PER_S) + j;
				return true;
			}
		}
	}
	
	return false;
}