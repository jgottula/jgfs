#include "jgfs.h"
#include <err.h>
#include <errno.h>
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

struct jgfs jgfs = {
	.hdr  = NULL,
	.rsvd = NULL,
	.fat  = NULL,
};

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
	
	jgfs.hdr = jgfs_get_sect(JGFS_HDR_SECT);
	
	if (new_hdr != NULL) {
		memset(jgfs.hdr, 0, SECT_SIZE);
		memcpy(jgfs.hdr, new_hdr, sizeof(*jgfs.hdr));
	}
	
	if (memcmp(jgfs.hdr->magic, JGFS_MAGIC, sizeof(jgfs.hdr->magic)) != 0) {
		errx(1, "jgfs header not found");
	}
	
	if (jgfs.hdr->ver_major != JGFS_VER_MAJOR ||
		jgfs.hdr->ver_minor != JGFS_VER_MINOR) {
		errx(1, "incompatible filesystem (%#06" PRIx16 ")",
			JGFS_VER_EXPAND(jgfs.hdr->ver_major, jgfs.hdr->ver_minor));
	}
	
	if (dev_sect < jgfs.hdr->s_total) {
		errx(1, "filesystem exceeds device bounds");
	}
	
	jgfs.rsvd = jgfs_get_sect(JGFS_HDR_SECT + 1);
	jgfs.fat  = jgfs_get_sect(jgfs.hdr->s_rsvd);
	
	clusters_total = (jgfs.hdr->s_total - (jgfs.hdr->s_rsvd +
		jgfs.hdr->s_fat)) / jgfs.hdr->s_per_c;
	
	if (jgfs.hdr->s_fat < CEIL(clusters_total, JGFS_FENT_PER_S)) {
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
	return (SECT_SIZE * jgfs.hdr->s_per_c);
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
	
	return jgfs_get_sect(jgfs.hdr->s_rsvd + jgfs.hdr->s_fat +
		(clust_num * jgfs.hdr->s_per_c));
}

fat_ent_t jgfs_fat_read(fat_ent_t addr) {
	uint16_t fat_sect = addr / JGFS_FENT_PER_S;
	uint16_t fat_idx  = addr % JGFS_FENT_PER_S;
	
	if (fat_sect >= jgfs.hdr->s_fat) {
		errx(1, "jgfs_fat_read: tried to access past s_fat "
			"(fat %#06" PRIx16 ")", addr);
	}
	
	return jgfs.fat[fat_sect].entries[fat_idx];
}

void jgfs_fat_write(fat_ent_t addr, fat_ent_t val) {
	uint16_t fat_sect = addr / JGFS_FENT_PER_S;
	uint16_t fat_idx  = addr % JGFS_FENT_PER_S;
	
	if (fat_sect >= jgfs.hdr->s_fat) {
		errx(1, "jgfs_fat_write: tried to access past s_fat "
			"(fat %#06" PRIx16 ")", addr);
	}
	
	jgfs.fat[fat_sect].entries[fat_idx] = val;
}

bool jgfs_find_free_clust(fat_ent_t *dest) {
	for (uint16_t i = 0; i < jgfs.hdr->s_fat; ++i) {
		for (uint16_t j = 0; j < JGFS_FENT_PER_S; ++j) {
			if (jgfs.fat[i].entries[j] == FAT_FREE) {
				*dest = (i * JGFS_FENT_PER_S) + j;
				return true;
			}
		}
	}
	
	return false;
}

uint16_t jgfs_count_fat(fat_ent_t target) {
	uint16_t count = 0;
	
	for (uint16_t i = 0; i < jgfs.hdr->s_fat; ++i) {
		for (uint16_t j = 0; j < JGFS_FENT_PER_S; ++j) {
			if (jgfs.fat[i].entries[j] == target) {
				++count;
			}
		}
	}
	
	return count;
}

int jgfs_lookup_child(const char *child_name, struct jgfs_dir_clust *parent,
	struct jgfs_dir_ent **child) {
	for (struct jgfs_dir_ent *this_ent = parent->entries;
		this_ent < parent->entries + JGFS_DENT_PER_C; ++this_ent) {
		if (strncmp(this_ent->name, child_name, JGFS_NAME_LIMIT) == 0) {
			*child = this_ent;
			return 0;
		}
	}
	
	/* TODO: try next cluster of directory, if present */
	
	return -ENOENT;
}

int jgfs_lookup(const char *path, struct jgfs_dir_clust **parent,
	struct jgfs_dir_ent **child) {
	bool find_child = (child != NULL);
	
	struct jgfs_dir_clust *dir_clust;
	struct jgfs_dir_ent   *dir_ent;
	
	dir_clust = jgfs_get_clust(FAT_ROOT);
	
	char *strtok_save;
	char *path_dup = strdup(path);
	char *path_part = strtok_r(path_dup, "/", &strtok_save);
	char *path_next;
	
	/* bail out before the last path component, but only if we weren't asked to
	 * find the child (we always at least find the parent) */
	while (path_part != NULL &&
		(path_next = strtok_r(NULL, "/", &strtok_save),
		!find_child && path_next != NULL)) {
		if (!jgfs_lookup_child(path_part, dir_clust, &dir_ent)) {
			free(path_dup);
			return -ENOENT;
		}
		
		if (dir_ent->type != TYPE_DIR) {
			return -ENOTDIR;
		}
		
		dir_clust = jgfs_get_clust(dir_ent->begin);
		path_part = path_next;
	}
	
	/* only assign to output pointers on success */
	*parent = dir_clust;
	if (find_child) {
		*child = dir_ent;
	}
	
	free(path_dup);
	return 0;
}
