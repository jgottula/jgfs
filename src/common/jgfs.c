#include "jgfs.h"
#include <bsd/string.h>
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
			JGFS_FENT_PER_S * new_hdr.s_per_c);
	} while (new_hdr.s_fat != s_fat);
	
	new_hdr.root_dir_ent.type  = TYPE_DIR;
	new_hdr.root_dir_ent.mtime = time(NULL);
	new_hdr.root_dir_ent.size  = SECT_SIZE * s_per_c;
	new_hdr.root_dir_ent.begin = FAT_ROOT;
	
	jgfs_init_real(dev_path, &new_hdr);
	
	for (uint16_t i = clusters_total;
		i < JGFS_FENT_PER_S * jgfs.hdr->s_fat; ++i) {
		jgfs.fat[i / JGFS_FENT_PER_S].entries[i % JGFS_FENT_PER_S] = FAT_OOB;
	}
	
	/* initialize the root directory cluster */
	struct jgfs_dir_clust *root_dir_clust = jgfs_get_clust(FAT_ROOT);
	jgfs_init_dir_clust(root_dir_clust, FAT_ROOT, FAT_ROOT);
	
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
	} else if (addr > clusters_total) {
		errx(1, "jgfs_fat_read: tried to access past clusters_total "
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
	} else if (addr > clusters_total) {
		errx(1, "jgfs_fat_write: tried to access past clusters_total "
			"(fat %#06" PRIx16 ")", addr);
	}
	
	jgfs.fat[fat_sect].entries[fat_idx] = val;
}

bool jgfs_find_free_clust(fat_ent_t start, fat_ent_t *dest) {
	for (uint16_t i = start / JGFS_FENT_PER_S; i < jgfs.hdr->s_fat; ++i) {
		for (uint16_t j = start % JGFS_FENT_PER_S; j < JGFS_FENT_PER_S; ++j) {
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

void jgfs_init_dir_clust(struct jgfs_dir_clust *dir_clust, fat_ent_t me,
	fat_ent_t parent) {
	memset(dir_clust, 0, jgfs_clust_size());
	dir_clust->me     = me;
	dir_clust->parent = parent;
}

int jgfs_lookup_child(const char *child_name, struct jgfs_dir_clust *parent,
	struct jgfs_dir_ent **child) {
	fat_ent_t parent_addr = parent->me;
	struct jgfs_dir_clust *parent_n = parent;
	
next_dir_clust:
	for (struct jgfs_dir_ent *this_ent = parent_n->entries;
		this_ent < parent_n->entries + JGFS_DENT_PER_C; ++this_ent) {
		if (strncmp(this_ent->name, child_name, JGFS_NAME_LIMIT + 1) == 0) {
			*child = this_ent;
			return 0;
		}
	}
	
	/* try the next cluster in the directory, if present */
	if ((parent_addr = jgfs_fat_read(parent_addr)) != FAT_EOF) {
		parent_n = jgfs_get_clust(parent_addr);
		goto next_dir_clust;
	}
	
	return -ENOENT;
}

int jgfs_lookup(const char *path, struct jgfs_dir_clust **parent,
	struct jgfs_dir_ent **child) {
	bool find_child = (child != NULL);
	
	struct jgfs_dir_clust *dir_clust;
	struct jgfs_dir_ent   *dir_ent;
	
	dir_clust = jgfs_get_clust(FAT_ROOT);
	dir_ent   = &jgfs.hdr->root_dir_ent;
	
	char *strtok_save = NULL;
	char *path_dup = strdup(path);
	char *path_part = strtok_r(path_dup, "/", &strtok_save);
	char *path_next;
	
	/* bail out before the last path component, but only if we weren't asked to
	 * find the child (we always at least find the parent) */
	while (path_part != NULL &&
		(path_next = strtok_r(NULL, "/", &strtok_save),
		find_child || path_next != NULL)) {
		
		if (jgfs_lookup_child(path_part, dir_clust, &dir_ent) != 0) {
			free(path_dup);
			return -ENOENT;
		}
		
		/* if we're still getting to the child, make sure we don't try to
		 * recurse into a non-directory */
		if (path_next != NULL) {
			if (dir_ent->type != TYPE_DIR) {
				return -ENOTDIR;
			}
			
			dir_clust = jgfs_get_clust(dir_ent->begin);
		}
		
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

uint32_t jgfs_dir_count_ents(struct jgfs_dir_clust *parent) {
	uint32_t count = 0;
	
	fat_ent_t parent_addr = parent->me;
	struct jgfs_dir_clust *parent_n = parent;
	
next_dir_clust:
	for (struct jgfs_dir_ent *this_ent = parent_n->entries;
		this_ent < parent_n->entries + JGFS_DENT_PER_C; ++this_ent) {
		if (this_ent->name[0] != '\0') {
			++count;
		}
	}
	
	/* try the next cluster in the directory, if present */
	if ((parent_addr = jgfs_fat_read(parent_addr)) != FAT_EOF) {
		parent_n = jgfs_get_clust(parent_addr);
		goto next_dir_clust;
	}
	
	return count;
}

int jgfs_dir_foreach(jgfs_dir_func_t func, struct jgfs_dir_clust *parent,
	void *user_ptr) {
	fat_ent_t parent_addr = parent->me;
	struct jgfs_dir_clust *parent_n = parent;
	
next_dir_clust:
	for (struct jgfs_dir_ent *this_ent = parent_n->entries;
		this_ent < parent_n->entries + JGFS_DENT_PER_C; ++this_ent) {
		if (this_ent->name[0] != '\0') {
			int rtn;
			if ((rtn = func(this_ent, user_ptr)) != 0) {
				return rtn;
			}
		}
	}
	
	/* try the next cluster in the directory, if present */
	if ((parent_addr = jgfs_fat_read(parent_addr)) != FAT_EOF) {
		parent_n = jgfs_get_clust(parent_addr);
		goto next_dir_clust;
	}
	
	return 0;
}

int jgfs_create_ent(struct jgfs_dir_clust *parent,
	const struct jgfs_dir_ent *new_ent, struct jgfs_dir_ent **created_ent) {
	if (strlen(new_ent->name) == 0) {
		errx(1, "jgfs_create_ent: new_ent->name is empty");
	}
	
	struct jgfs_dir_ent *old_ent;
	if (jgfs_lookup_child(new_ent->name, parent, &old_ent) == 0) {
		return -EEXIST;
	}
	
	fat_ent_t parent_addr = parent->me;
	struct jgfs_dir_clust *parent_n = parent;
	
	struct jgfs_dir_ent *empty_ent;
next_dir_clust:
	for (struct jgfs_dir_ent *this_ent = parent_n->entries;
		this_ent < parent_n->entries + JGFS_DENT_PER_C; ++this_ent) {
		if (this_ent->name[0] == '\0') {
			empty_ent = this_ent;
			goto found;
		}
	}
	
	/* try the next cluster in the directory, if present */
	if ((parent_addr = jgfs_fat_read(parent_addr)) != FAT_EOF) {
		parent_n = jgfs_get_clust(parent_addr);
		goto next_dir_clust;
	}
	
	/* TODO: on failure to find an empty entry, extend the directory to another
	 * cluster and increase its size accordingly */
	/* NOTE: this will require finding the parent's parent so we can increase
	 * the parent's size */
	/* NOTE: this means we need to specially handle the root dir */
	
	/* TODO: on failure to allocate a new cluster, return -ENOSPC */
	return -ENOSPC;
	
found:
	memcpy(empty_ent, new_ent, sizeof(*empty_ent));
	
	if (created_ent != NULL) {
		*created_ent = empty_ent;
	}
	
	return 0;
}

int jgfs_create_file(struct jgfs_dir_clust *parent, const char *name) {
	if (strlen(name) > JGFS_NAME_LIMIT) {
		return -ENAMETOOLONG;
	}
	
	struct jgfs_dir_ent new_ent;
	memset(&new_ent, 0, sizeof(new_ent));
	strlcpy(new_ent.name, name, JGFS_NAME_LIMIT + 1);
	new_ent.type  = TYPE_FILE;
	new_ent.attr  = ATTR_NONE;
	new_ent.mtime = time(NULL);
	new_ent.size  = 0;
	new_ent.begin = FAT_NALLOC;
	
	return jgfs_create_ent(parent, &new_ent, NULL);
}

int jgfs_create_dir(struct jgfs_dir_clust *parent, const char *name) {
	if (strlen(name) > JGFS_NAME_LIMIT) {
		return -ENAMETOOLONG;
	}
	
	struct jgfs_dir_ent new_ent, *created_ent;
	memset(&new_ent, 0, sizeof(new_ent));
	strlcpy(new_ent.name, name, JGFS_NAME_LIMIT + 1);
	new_ent.type  = TYPE_DIR;
	new_ent.attr  = ATTR_NONE;
	new_ent.mtime = time(NULL);
	new_ent.size  = jgfs_clust_size();
	new_ent.begin = FAT_NALLOC;
	
	int rtn;
	if ((rtn = jgfs_create_ent(parent, &new_ent, &created_ent)) != 0) {
		return rtn;
	}
	
	fat_ent_t dest_addr;
	if (!jgfs_find_free_clust(FAT_ROOT, &dest_addr)) {
		/* if we can't allocate a cluster for the directory, go back and
		 * delete the directory entry so things are in a consistent state */
		if ((errno = jgfs_delete_ent(parent, name)) != 0) {
			err(1, "jgfs_create_dir: could not undo dir ent creation");
		}
		
		return -ENOSPC;
	}
	
	created_ent->begin = dest_addr;
	
	struct jgfs_dir_clust *dir_clust = jgfs_get_clust(dest_addr);
	jgfs_init_dir_clust(dir_clust, dest_addr, parent->me);
	
	jgfs_fat_write(dest_addr, FAT_EOF);
	
	return 0;
}

int jgfs_create_symlink(struct jgfs_dir_clust *parent, const char *name,
	const char *target) {
	if (strlen(name) > JGFS_NAME_LIMIT ||
		strlen(target) > jgfs_clust_size() - 1) {
		return -ENAMETOOLONG;
	}
	
	struct jgfs_dir_ent new_ent, *created_ent;
	memset(&new_ent, 0, sizeof(new_ent));
	strlcpy(new_ent.name, name, JGFS_NAME_LIMIT + 1);
	new_ent.type  = TYPE_SYMLINK;
	new_ent.attr  = ATTR_NONE;
	new_ent.mtime = time(NULL);
	new_ent.size  = strlen(target);
	new_ent.begin = FAT_NALLOC;
	
	int rtn;
	if ((rtn = jgfs_create_ent(parent, &new_ent, &created_ent)) != 0) {
		return rtn;
	}
	
	fat_ent_t dest_addr;
	if (!jgfs_find_free_clust(FAT_ROOT, &dest_addr)) {
		/* if we can't allocate a cluster for the symlink, go back and
		 * delete the directory entry so things are in a consistent state */
		if ((errno = jgfs_delete_ent(parent, name)) != 0) {
			err(1, "jgfs_create_symlink: could not undo dir ent creation");
		}
		
		return -ENOSPC;
	}
	
	created_ent->begin = dest_addr;
	
	char *symlink_clust = jgfs_get_clust(dest_addr);
	strlcpy(symlink_clust, target, jgfs_clust_size());
	
	jgfs_fat_write(dest_addr, FAT_EOF);
	
	return 0;
}

int jgfs_delete_ent(struct jgfs_dir_clust *parent, const char *name) {
	struct jgfs_dir_ent *child;
	int rtn;
	if ((rtn = jgfs_lookup_child(name, parent, &child)) != 0) {
		return rtn;
	}
	
	/* check for directory emptiness, if appropriate */
	if (child->type == TYPE_DIR) {
		struct jgfs_dir_clust *dir_clust = jgfs_get_clust(child->begin);
		if (jgfs_dir_count_ents(dir_clust) != 0) {
			return -ENOTEMPTY;
		}
	}
	
	/* deallocate all the clusters associated with the dir ent */
	if (child->size != 0 && (rtn = jgfs_reduce(child, 0)) != 0) {
		return rtn;
	}
	
	/* erase this dir ent from the parent dir cluster */
	memset(child, 0, sizeof(*child));
	
	/* if we erased the last dir ent in a dir cluster, deallocate it */
	jgfs_condense_dir_clust(parent);
	
	return 0;
}

void jgfs_condense_dir_clust(struct jgfs_dir_clust *dir_clust) {
	/* TODO:
	 * - check for clusters in this dir that have no entries and delete them
	 *   - but NOT if it is the only cluster in that directory
	 *   - set the previous cluster's fat value (or the parent dir_ent's begin
	 *     value) to what the empty cluster's fat value was (EOF or otherwise)
	 *   - update all the parent's dir clusters to have 'me' set to the new
	 *     begin value if it was modified
	 *   - update the parent dir_ent's size
	 */
	
	warnx("jgfs_condense_dir_clust: not implemented");
}

int jgfs_reduce(struct jgfs_dir_ent *dir_ent, uint32_t new_size) {
	if (new_size >= dir_ent->size) {
		errx(1, "jgfs_reduce: new_size is not smaller");
	}
	
	uint16_t clust_before = CEIL(dir_ent->size, jgfs_clust_size()),
		clust_after = CEIL(new_size, jgfs_clust_size());
	
	if (clust_before != clust_after) {
		fat_ent_t this = dir_ent->begin, next;
		
		for (uint16_t i = 1; i <= clust_before; ++i) {
			next = jgfs_fat_read(this);
			
			if (i == clust_after) {
				jgfs_fat_write(this, FAT_EOF);
			} else if (i > clust_after) {
				jgfs_fat_write(this, FAT_FREE);
			}
			
			/* this means the filesystem is inconsistent */
			if (next == FAT_EOF && i < clust_before) {
				warnx("jgfs_reduce: found premature FAT_EOF in clust chain");
				break;
			}
			
			this = next;
		}
		
		/* special case for zero-size files */
		if (clust_after == 0) {
			jgfs_fat_write(dir_ent->begin, FAT_FREE);
			dir_ent->begin = FAT_NALLOC;
		}
	}
	
	dir_ent->size = new_size;
	
	return 0;
}

int jgfs_enlarge(struct jgfs_dir_ent *dir_ent, uint32_t new_size) {
	if (new_size >= dir_ent->size) {
		errx(1, "jgfs_enlarge: new_size is not larger");
	}
	
	
	
	return -ENOSYS;
}
