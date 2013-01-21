#include "fs.h"
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../common/jgfs.h"
#include "../common/version.h"


#define CLUSTER(x) (hdr.sz_rsvd + hdr.sz_fat + x)
#define GET_BLOCKS(x) (((x - 1) / 512) + 1)


struct fuse_operations jgfs_oper = {
	.getattr  = jgfs_getattr,
	.mknod    = jgfs_mknod,
	.unlink   = jgfs_unlink,
	.rmdir    = jgfs_rmdir,
	.readlink = jgfs_readlink,
	.open     = jgfs_open,
	.read     = jgfs_read,
	.statfs   = jgfs_statfs,
	.readdir  = jgfs_readdir,
	.init     = jgfs_init,
	.destroy  = jgfs_destroy,
};

extern char *dev_path;
int dev_fd = -1;

struct jgfs_header      hdr;
struct jgfs_fat_sector *fat;
struct jgfs_dir_cluster root_dir;


void read_sector(uint16_t sect, void *data) {
	ssize_t b_read;
	
	lseek(dev_fd, sect * 0x200, SEEK_SET);
	
	switch ((b_read = read(dev_fd, data, 512))) {
	case -1:
		err(1, "read_sector #%" PRIu16 " failed", sect);
	case 512:
		warnx("read_sector #%" PRIu16, sect);
		break;
	default:
		errx(1, "read_sector #%" PRIu16 " incomplete: %zd/512 bytes",
			sect, b_read);
	}
}

void write_sector(uint16_t sect, const void *data) {
	ssize_t b_written;
	
	lseek(dev_fd, sect * 0x200, SEEK_SET);
	
	switch ((b_written = write(dev_fd, data, 512))) {
	case -1:
		err(1, "write_sector #%" PRIu16 " failed", sect);
	case 512:
		warnx("write_sector #%" PRIu16, sect);
		break;
	default:
		errx(1, "write_sector #%" PRIu16 " incomplete: %zd/512 bytes",
			sect, b_written);
	}
}

fat_ent_t read_fat(fat_ent_t addr) {
	struct jgfs_fat_sector fat_sector;
	
	warnx("read_fat 0x%04x", addr);
	
	read_sector(hdr.sz_rsvd + (addr / 256), &fat_sector);
	
	return fat_sector.entries[addr % 256];
}

void write_fat(fat_ent_t addr, fat_ent_t value) {
	struct jgfs_fat_sector fat_sector;
	
	warnx("write_fat 0x%04x: 0x%04X", addr, value);
	
	read_sector(hdr.sz_rsvd + (addr / 256), &fat_sector);
	fat_sector.entries[addr % 256] = value;
	write_sector(hdr.sz_rsvd + (addr / 256), &fat_sector);
}

bool find_free_cluster(fat_ent_t *addr) {
	for (uint16_t i = 0; i < hdr.sz_fat; ++i) {
		struct jgfs_fat_sector fat_sector;
		read_sector(hdr.sz_rsvd + i, &fat_sector);
		
		for (uint16_t j = 0; j < 256; ++j) {
			if (fat_sector.entries[j] == FAT_FREE) {
				*addr = (i * 256) + j;
				return true;
			}
		}
	}
	
	return false;
}

int lookup_path(const char *path, struct jgfs_dir_entry *dir_ent) {
	struct jgfs_dir_cluster dir_cluster;
	char *path_dup, *path_part;
	
	warnx("lookup_path %s", path);
	
	path_dup = strdup(path);
	
	/* the root directory doesn't have an actual entry */
	memset(dir_ent, 0, sizeof(*dir_ent));
	dir_ent->size   = 512;
	dir_ent->attrib = ATTR_DIR;
	
	path_part = strtok(path_dup, "/");
	while (path_part != NULL) {
		read_sector(CLUSTER(dir_ent->begin), &dir_cluster);
		
		for (struct jgfs_dir_entry *this_ent = dir_cluster.entries;
			this_ent < dir_cluster.entries + 15; ++this_ent) {
			if (strcmp(this_ent->name, path_part) == 0) {
				*dir_ent = *this_ent;
				goto success;
			}
		}
		
		/* is this the right error code? */
		warnx("lookup_path: entry not found!");
		free(path_dup);
		return -ENOENT;
		
	success:
		path_part = strtok(NULL, "/");
	}
	
	free(path_dup);
	return 0;
}

int lookup_parent(const char *path, struct jgfs_dir_entry *dir_ent) {
	char *path_dup = strdup(path);
	
	/* terminate the string after the last '/' */
	strrchr(path_dup, '/')[1] = '\0';
	
	int rtn = lookup_path(path_dup, dir_ent);
	
	free(path_dup);
	
	return rtn;
}

int jgfs_getattr(const char *path, struct stat *buf) {
	struct jgfs_dir_entry dir_ent;
	int rtn = lookup_path(path, &dir_ent);
	
	if (rtn != 0) {
		return rtn;
	}
	
	buf->st_nlink = 1;
	buf->st_uid = 0;
	buf->st_gid = 0;
	buf->st_size = dir_ent.size;
	buf->st_blocks = GET_BLOCKS(dir_ent.size);
	buf->st_atime = buf->st_ctime = buf->st_mtime = dir_ent.mtime;
	
	if (dir_ent.attrib & ATTR_FILE) {
		buf->st_mode = 0644 | S_IFREG;
	} else if (dir_ent.attrib & ATTR_DIR) {
		buf->st_mode = 0755 | S_IFDIR;
	} else if (dir_ent.attrib & ATTR_SYMLINK) {
		buf->st_mode = 0777 | S_IFLNK;
	} else {
		errx(1, "jgfs_getattr: unknown attrib 0x%x", dir_ent.attrib);
	}
	
	return 0;
}

int jgfs_mknod(const char *path, mode_t mode, dev_t dev) {
	const char *path_last = strrchr(path, '/') + 1;
	
	if (strlen(path_last) > 19) {
		return -ENAMETOOLONG;
	}
	
	struct jgfs_dir_entry parent_ent;
	int rtn = lookup_parent(path, &parent_ent);
	if (rtn != 0) {
		return rtn;
	}
	
	struct jgfs_dir_entry *new_ent = NULL;
	struct jgfs_dir_cluster parent_cluster;
	read_sector(CLUSTER(parent_ent.begin), &parent_cluster);
	
	/* find an empty directory entry, and check for entry with same name */
	for (struct jgfs_dir_entry *this_ent = parent_cluster.entries;
		this_ent < parent_cluster.entries + 15; ++this_ent) {
		if (this_ent->name[0] == '\0') {
			new_ent = this_ent;
		} else if (strcmp(path_last, this_ent->name) == 0) {
			return -EEXIST;
		}
	}
	
	/* directory is full */
	if (new_ent == NULL) {
		return -ENOSPC;
	}
	
	memset(new_ent, 0, sizeof(*new_ent));
	strcpy(new_ent->name, path_last);
	new_ent->mtime = time(NULL);
	new_ent->size  = 0;
	new_ent->begin = 0;
	
	switch (mode & ~0777) {
	case 0:
	case S_IFREG:
		new_ent->attrib = ATTR_FILE;
		break;
	default:
		return -EPERM;
	}
	
	write_sector(CLUSTER(parent_ent.begin), &parent_cluster);
	
	return 0;
}

int jgfs_mkdir(const char *path, mode_t mode) {
	
}

int jgfs_unlink(const char *path) {
	const char *path_last = strrchr(path, '/') + 1;
	
	struct jgfs_dir_entry dir_ent;
	int rtn = lookup_path(path, &dir_ent);
	if (rtn != 0) {
		return rtn;
	}
	
	/* sanity check */
	if (dir_ent.attrib == ATTR_DIR) {
		return -EISDIR;
	}
	
	/* free up the file's clusters */
	if (dir_ent.size != 0) {
		fat_ent_t data_addr = dir_ent.begin, old_value;
		
		do {
			old_value = read_fat(data_addr);
			write_fat(data_addr, FAT_FREE);
			
			data_addr = old_value;
		} while (old_value != FAT_EOF);
	}
	
	struct jgfs_dir_entry parent_ent;
	if ((rtn = lookup_parent(path, &parent_ent)) != 0) {
		return rtn;
	}
	
	struct jgfs_dir_cluster dir_cluster;
	read_sector(CLUSTER(parent_ent.begin), &dir_cluster);
	
	/* delete this file's entry from its parent */
	for (struct jgfs_dir_entry *this_ent = dir_cluster.entries;
		this_ent < dir_cluster.entries + 15; ++this_ent) {
		if (strcmp(path_last, this_ent->name) == 0) {
			memset(this_ent, 0, sizeof(*this_ent));
			break;
		}
	}
	
	write_sector(CLUSTER(parent_ent.begin), &dir_cluster);
	
	return 0;
}

int jgfs_rmdir(const char *path) {
	const char *path_last = strrchr(path, '/') + 1;
	
	struct jgfs_dir_entry dir_ent;
	int rtn = lookup_path(path, &dir_ent);
	if (rtn != 0) {
		return rtn;
	}
	
	/* sanity check */
	if (dir_ent.attrib != ATTR_DIR) {
		return -ENOTDIR;
	}
	
	struct jgfs_dir_cluster dir_cluster;
	read_sector(CLUSTER(dir_ent.begin), &dir_cluster);
	
	/* ensure directory emptiness */
	for (struct jgfs_dir_entry *this_ent = dir_cluster.entries;
		this_ent < dir_cluster.entries + 15; ++this_ent) {
		if (this_ent->name[0] != '\0') {
			return -ENOTEMPTY;
		}
	}
	
	read_sector(CLUSTER(dir_cluster.parent), &dir_cluster);
	
	/* delete this dir's entry from its parent */
	for (struct jgfs_dir_entry *this_ent = dir_cluster.entries;
		this_ent < dir_cluster.entries + 15; ++this_ent) {
		if (strcmp(path_last, this_ent->name) == 0) {
			memset(this_ent, 0, sizeof(*this_ent));
			break;
		}
	}
	
	write_sector(CLUSTER(dir_cluster.me), &dir_cluster);
	
	/* free up this dir's cluster */
	write_fat(dir_ent.begin, FAT_FREE);
	
	return 0;
}

int jgfs_symlink(const char *path, const char *link) {
	
}

int jgfs_rename(const char *path, const char *newpath) {
	
}

int jgfs_readlink(const char *path, char *link, size_t size) {
	struct jgfs_dir_entry dir_ent;
	int rtn = lookup_path(path, &dir_ent);
	
	if (rtn != 0) {
		return rtn;
	} else if (dir_ent.attrib != ATTR_SYMLINK) {
		errx(1, "jgfs_readlink: wrong attrib 0x%x", dir_ent.attrib);
	}
	
	uint8_t buffer[512];
	read_sector(CLUSTER(dir_ent.begin), buffer);
	
	memset(link, 0, size);
	
	if (dir_ent.size >= size) {
		memcpy(link, buffer, size - 1);
	} else {
		memcpy(link, buffer, dir_ent.size);
	}
	
	return 0;
}

int jgfs_open(const char *path, struct fuse_file_info *fi) {
	return 0;
}

int jgfs_read(const char *path, char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi) {
	fat_ent_t data_addr;
	uint32_t file_size;
	int b_read = 0;
	
	struct jgfs_dir_entry dir_ent;
	int rtn = lookup_path(path, &dir_ent);
	if (rtn != 0) {
		return rtn;
	}
	
	file_size = dir_ent.size;
	
	memset(buf, 0, size);
	
	/* EOF */
	if (file_size <= offset) {
		return 0;
	}
	
	/* skip to the first cluster requested */
	data_addr = dir_ent.begin;
	while (offset >= 512) {
		data_addr  = read_fat(data_addr);
		offset    -= 512;
		file_size -= 512;
	}
	
	while (size > 0 && file_size > 0) {
		uint16_t size_this_cluster;
		
		if (size < file_size) {
			size_this_cluster = size;
		} else {
			size_this_cluster = file_size;
		}
		
		if (size_this_cluster > (512 - offset)) {
			size_this_cluster = (512 - offset);
		}
		
		uint8_t data_buf[512];
		read_sector(CLUSTER(data_addr), data_buf);
		memcpy(buf, data_buf + offset, size_this_cluster);
		
		buf       += size_this_cluster;
		b_read    += size_this_cluster;
		
		size      -= size_this_cluster;
		file_size -= size_this_cluster;
		
		/* next cluster */
		data_addr = read_fat(data_addr);
	}
	
	return b_read;
}

int jgfs_statfs(const char *path, struct statvfs *statv) {
	uint16_t sz_free = 0;
	
	for (uint16_t i = 0; i < hdr.sz_fat; ++i) {
		struct jgfs_fat_sector fat_sector;
		read_sector(hdr.sz_rsvd + i, &fat_sector);
		
		for (uint16_t j = 0; j < 256; ++j) {
			if (fat_sector.entries[j] == FAT_FREE) {
				++sz_free;
			}
		}
	}
	
	memset(statv, 0, sizeof(*statv));
	
	statv->f_bsize = 512;
	statv->f_blocks = hdr.sz_total;
	statv->f_bfree = statv->f_bavail = sz_free;
	
	statv->f_namemax = 19;
	
	return 0;
}

int jgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi) {
	struct jgfs_dir_cluster dir_cluster;
	struct jgfs_dir_entry dir_ent;
	int rtn = lookup_path(path, &dir_ent);
	
	if (rtn != 0) {
		return rtn;
	}
	
	if (!(dir_ent.attrib & ATTR_DIR)) {
		warnx("jgfs_readdir: not a directory!");
		return -ENOTDIR;
	}
	
	read_sector(CLUSTER(dir_ent.begin), &dir_cluster);
	
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	
	for (struct jgfs_dir_entry *this_ent = dir_cluster.entries;
		this_ent < dir_cluster.entries + 15; ++this_ent) {
		if (strlen(this_ent->name) != 0) {
			filler(buf, this_ent->name, NULL, 0);
		}
	}
	
	return 0;
}

void *jgfs_init(struct fuse_conn_info *conn) {
	if ((dev_fd = open(dev_path, O_RDWR)) == -1) {
		err(1, "failed to open %s", dev_path);
	}
	
	read_sector(1, &hdr);
	
	if (hdr.ver_major != JGFS_VER_MAJOR || hdr.ver_minor != JGFS_VER_MINOR) {
		close(dev_fd);
		errx(1, "fs has wrong version (0x%02x%02x)",
			hdr.ver_major, hdr.ver_minor);
	}
	
	fat = malloc(hdr.sz_fat * sizeof(struct jgfs_fat_sector));
	for (uint16_t i = 0; i < hdr.sz_fat; ++i) {
		read_sector(hdr.sz_rsvd + i, fat + i);
	}
	
	return NULL;
}

void jgfs_destroy(void *userdata) {
	close(dev_fd);
	
	free(fat);
}
