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
		break;
	default:
		errx(1, "write_sector #%" PRIu16 " incomplete: %zd/512 bytes",
			sect, b_written);
	}
}

int lookup_file(const char *path, struct jgfs_dir_entry *dir_ent) {
	struct jgfs_dir_entry root_dir_ent;
	struct jgfs_dir_cluster dir_cluster;
	char *path_part;
	
	printf("lookup_file %s\n", path);
	
	/* the root directory doesn't have an actual entry */
	memset(&root_dir_ent, 0, sizeof(root_dir_ent));
	root_dir_ent.size   = 512;
	root_dir_ent.attrib = FILE_DIR;
	
	*dir_ent = root_dir_ent;
	
	path_part = strtok((char *)path, "/");
	while (path_part != NULL) {
		read_sector(CLUSTER(dir_ent->begin), &dir_cluster);
		
		if (strcmp(path_part, ".") == 0) {
			goto success;
		} else if (strcmp(path_part, "..") == 0) {
			/* stop at the root dir */
			if (dir_cluster.parent == 0) {
				*dir_ent = root_dir_ent;
				goto success;
			}
			
			/* because the metadata for a given directory is located in its
			 * parent's cluster, we must go up two steps and then back down one
			 * to find the right directory entry */
			read_sector(CLUSTER(dir_cluster.parent), &dir_cluster);
			fat_ent_t target = dir_cluster.me;
			read_sector(CLUSTER(dir_cluster.parent), &dir_cluster);
			
			for (struct jgfs_dir_entry *this_ent = dir_cluster.entries;
				this_ent < dir_cluster.entries + 15; ++this_ent) {
				if ((this_ent->attrib & FILE_DIR) &&
					this_ent->begin == target) {
					*dir_ent = *this_ent;
					goto success;
				}
			}
			
			/* this really should not happen */
			errx(1, "lookup_file: catastrophic failure in '..'");
		}
		
		for (struct jgfs_dir_entry *this_ent = dir_cluster.entries;
			this_ent < dir_cluster.entries + 15; ++this_ent) {
			if (strcmp(this_ent->name, path_part) == 0) {
				// if this is a file and not the last component,
				// then this is an invalid path
				// (need to handle this!)
				
				*dir_ent = *this_ent;
				goto success;
			}
		}
		
		/* is this the right error code? */
		warnx("lookup_file: entry not found!");
		return -ENOENT;
		
	success:
		path_part = strtok(NULL, "/");
	}
	
	return 0;
}

int jgfs_getattr(const char *path, struct stat *buf) {
	struct jgfs_dir_entry dir_ent;
	int rtn = lookup_file(path, &dir_ent);
	
	if (rtn != 0) {
		return rtn;
	}
	
	buf->st_mode = 0777;
	buf->st_nlink = 1;
	buf->st_uid = 0;
	buf->st_gid = 0;
	buf->st_size = dir_ent.size;
	buf->st_blocks = GET_BLOCKS(dir_ent.size);
	buf->st_atime = buf->st_ctime = buf->st_mtime = time(NULL);
	
	if (dir_ent.attrib & FILE_DIR) {
		buf->st_mode |= S_IFDIR;
	}
	
	return 0;
}

int jgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi) {
	struct jgfs_dir_cluster dir_cluster;
	struct jgfs_dir_entry dir_ent;
	int rtn = lookup_file(path, &dir_ent);
	
	if (rtn != 0) {
		return rtn;
	}
	
	if (!(dir_ent.attrib & FILE_DIR)) {
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
