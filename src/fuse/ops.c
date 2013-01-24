#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION  26
#include <bsd/string.h>
#include <err.h>
#include <errno.h>
#include <fuse.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/jgfs.h"


char *dev_path;


void *jg_init(struct fuse_conn_info *conn) {
	jgfs_init(dev_path);
	
	return NULL;
}

void jg_destroy(void *userdata) {
	jgfs_done();
}

int jg_statfs(const char *path, struct statvfs *statv) {
	memset(statv, 0, sizeof(*statv));
	
	statv->f_bsize = jgfs_clust_size();
	statv->f_blocks = jgfs.hdr->s_total / jgfs.hdr->s_per_c;
	statv->f_bfree = statv->f_bavail = jgfs_count_fat(FAT_FREE);
	
	statv->f_namemax = JGFS_NAME_LIMIT;
	
	return 0;
}

int jg_getattr(const char *path, struct stat *buf) {
	struct jgfs_dir_clust *parent;
	struct jgfs_dir_ent   *child;
	int rtn;
	if ((rtn = jgfs_lookup(path, &parent, &child)) != 0) {
		return rtn;
	}
	
	buf->st_nlink = 1;
	buf->st_uid = 0;
	buf->st_gid = 0;
	buf->st_size = child->size;
	buf->st_blocks = CEIL(child->size, jgfs_clust_size());
	buf->st_atime = buf->st_ctime = buf->st_mtime = child->mtime;
	
	if (child->type == TYPE_FILE) {
		buf->st_mode = 0644 | S_IFREG;
	} else if (child->type == TYPE_DIR) {
		buf->st_mode = 0755 | S_IFDIR;
	} else if (child->type == TYPE_SYMLINK) {
		buf->st_mode = 0777 | S_IFLNK;
	} else {
		errx(1, "jgfs_getattr: unknown type 0x%x", child->type);
	}
	
	return 0;
}

int jg_utimens(const char *path, const struct timespec tv[2]) {
	struct jgfs_dir_clust *parent;
	struct jgfs_dir_ent   *child;
	int rtn;
	if ((rtn = jgfs_lookup(path, &parent, &child)) != 0) {
		return rtn;
	}
	
	child->mtime = tv[1].tv_sec;
	
	return 0;
}

int jg_chmod(const char *path, mode_t mode) {
	struct jgfs_dir_clust *parent;
	struct jgfs_dir_ent   *child;
	int rtn;
	if ((rtn = jgfs_lookup(path, &parent, &child)) != 0) {
		return rtn;
	}
	
	return 0;
}

int jg_chown(const char *path, uid_t uid, gid_t gid) {
	struct jgfs_dir_clust *parent;
	struct jgfs_dir_ent   *child;
	int rtn;
	if ((rtn = jgfs_lookup(path, &parent, &child)) != 0) {
		return rtn;
	}
	
	return 0;
}

int jg_fsync(const char *path, int datasync, struct fuse_file_info *fi) {
	/* just flush everything */
	jgfs_sync();
	
	return 0;
}

int jg_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi) {
	/* just flush everything */
	jgfs_sync();
	
	return 0;
}

int jg_readdir_filler(struct jgfs_dir_ent *dir_ent, void *user_ptr) {
	void *buf = ((void **)user_ptr)[0];
	fuse_fill_dir_t filler = ((void **)user_ptr)[1];
	
	if (filler(buf, dir_ent->name, NULL, 0) == 1) {
		return -EINVAL;
	}
	
	return 0;
}

int jg_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi) {
	struct jgfs_dir_clust *parent;
	struct jgfs_dir_ent   *child;
	int rtn;
	if ((rtn = jgfs_lookup(path, &parent, &child)) != 0) {
		return rtn;
	}
	
	if (child->type != TYPE_DIR) {
		return -ENOTDIR;
	}
	
	/* we want the child's dir clust, not the parent's */
	struct jgfs_dir_clust *dir_clust = jgfs_get_clust(child->begin);
	
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	
	void *filler_info[] = {
		buf,
		filler,
	};
	
	return jgfs_dir_foreach(jg_readdir_filler, dir_clust, filler_info);
}

int jg_readlink(const char *path, char *link, size_t size) {
	struct jgfs_dir_clust *parent;
	struct jgfs_dir_ent   *child;
	int rtn;
	if ((rtn = jgfs_lookup(path, &parent, &child)) != 0) {
		return rtn;
	}
	
	memset(link, 0, size);
	
	struct clust *link_sect = jgfs_get_clust(child->begin);
	
	if (size < child->size) {
		memcpy(link, link_sect, size);
	} else {
		memcpy(link, link_sect, child->size);
	}
	
	return 0;
}

int jg_mknod(const char *path, mode_t mode, dev_t dev) {
	struct jgfs_dir_clust *parent;
	int rtn;
	if ((rtn = jgfs_lookup(path, &parent, NULL)) != 0) {
		return rtn;
	}
	
	switch (mode & ~0777) {
	case 0:
	case S_IFREG:
		break;
	default:
		return -EPERM;
	}
	
	char *path_last = strrchr(path, '/') + 1;
	if (path_last == NULL || path_last[0] == '\0') {
		/* most applicable errno */
		return -EINVAL;
	}
	
	return jgfs_create_file(parent, path_last);
}

int jg_mkdir(const char *path, mode_t mode) {
	struct jgfs_dir_clust *parent;
	int rtn;
	if ((rtn = jgfs_lookup(path, &parent, NULL)) != 0) {
		return rtn;
	}
	
	char *path_last = strrchr(path, '/') + 1;
	if (path_last == NULL || path_last[0] == '\0') {
		/* most applicable errno */
		return -EINVAL;
	}
	
	return jgfs_create_dir(parent, path_last);
}

int jg_unlink(const char *path) {
	struct jgfs_dir_clust *parent;
	struct jgfs_dir_ent   *child;
	int rtn;
	if ((rtn = jgfs_lookup(path, &parent, &child)) != 0) {
		return rtn;
	}
	
	if (child->type == TYPE_DIR) {
		return -EISDIR;
	}
	
	return jgfs_delete_ent(parent, child->name, true);
}

int jg_rmdir(const char *path) {
	struct jgfs_dir_clust *parent;
	struct jgfs_dir_ent   *child;
	int rtn;
	if ((rtn = jgfs_lookup(path, &parent, &child)) != 0) {
		return rtn;
	}
	
	if (child->type != TYPE_DIR) {
		return -ENOTDIR;
	}
	
	return jgfs_delete_ent(parent, child->name, true);
}

int jg_symlink(const char *target, const char *path) {
	struct jgfs_dir_clust *parent;
	int rtn;
	if ((rtn = jgfs_lookup(path, &parent, NULL)) != 0) {
		return rtn;
	}
	
	char *path_last = strrchr(path, '/') + 1;
	if (path_last == NULL || path_last[0] == '\0') {
		/* most applicable errno */
		return -EINVAL;
	}
	
	return jgfs_create_symlink(parent, path_last, target);
}

int jg_rename(const char *path, const char *newpath) {
	char *newpath_last = strrchr(newpath, '/') + 1;
	if (newpath_last == NULL || newpath_last[0] == '\0') {
		/* most applicable errno */
		return -EINVAL;
	} else if (strlen(newpath_last) > JGFS_NAME_LIMIT) {
		return -ENAMETOOLONG;
	}
	
	struct jgfs_dir_clust *old_parent, *new_parent;
	struct jgfs_dir_ent *dir_ent;
	int rtn;
	if ((rtn = jgfs_lookup(path, &old_parent, &dir_ent)) != 0 ||
		(rtn = jgfs_lookup(newpath, &new_parent, NULL)) != 0) {
		return rtn;
	}
	
	/* rename the dir ent */
	strlcpy(dir_ent->name, newpath_last, JGFS_NAME_LIMIT + 1);
	
	/* transplant it (even if it's the same directory) */
	return jgfs_move_ent(dir_ent, new_parent);
}

int jg_open(const char *path, struct fuse_file_info *fi) {
	return 0;
}

int jg_truncate(const char *path, off_t newsize) {
	struct jgfs_dir_clust *parent;
	struct jgfs_dir_ent   *child;
	int rtn;
	if ((rtn = jgfs_lookup(path, &parent, &child)) != 0) {
		return rtn;
	}
	
	if (child->type != TYPE_FILE) {
		return -EISDIR;
	}
	
	child->mtime = time(NULL);
	
	if (newsize < child->size) {
		return jgfs_reduce(child, newsize);
	} else if (newsize > child->size) {
		return jgfs_enlarge(child, newsize);
	} else {
		return 0;
	}
}

int jg_ftruncate(const char *path, off_t newsize, struct fuse_file_info *fi) {
	return jg_truncate(path, newsize);
}

int jg_read(const char *path, char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi) {
	struct jgfs_dir_clust *parent;
	struct jgfs_dir_ent   *child;
	int rtn;
	if ((rtn = jgfs_lookup(path, &parent, &child)) != 0) {
		return rtn;
	}
	
	memset(buf, 0, size);
	
	uint32_t file_size = child->size;
	int b_read = 0;
	
	/* immediate EOF check */
	if (file_size <= offset) {
		return 0;
	}
	
	/* skip to the first cluster requested */
	fat_ent_t data_addr = child->begin;
	while (offset >= jgfs_clust_size()) {
		data_addr  = jgfs_fat_read(data_addr);
		offset    -= jgfs_clust_size();
		file_size -= jgfs_clust_size();
	}
	
	while (size > 0 && file_size > 0) {
		uint32_t size_this_cluster;
		
		if (size < file_size) {
			size_this_cluster = size;
		} else {
			size_this_cluster = file_size;
		}
		
		/* read to the end of this cluster on this iteration */
		if (size_this_cluster > (jgfs_clust_size() - offset)) {
			size_this_cluster = (jgfs_clust_size() - offset);
		}
		
		struct clust *data_clust = jgfs_get_clust(data_addr);
		memcpy(buf, (char *)data_clust + offset, size_this_cluster);
		
		buf       += size_this_cluster;
		b_read    += size_this_cluster;
		
		size      -= size_this_cluster;
		file_size -= size_this_cluster;
		
		/* next cluster */
		data_addr = jgfs_fat_read(data_addr);
	}
	
	return b_read;
}

/* check for cases: (using 2K file)
 * append 512 @ 2048
 * overwrite 512 @ 1024
 * overwrite/append 1024 @ 1536
 * append 512 @ 3072
 */
int jg_write(const char *path, const char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi) {
	return -ENOSYS;
#if 0
	fat_ent_t data_addr;
	int b_written = 0;
	
	struct jgfs_dir_entry dir_ent;
	int rtn = lookup_path(path, &dir_ent);
	if (rtn != 0) {
		return rtn;
	}
	
	uint32_t file_size = dir_ent.size;
	
	/* if offset is greater than file_size, call jgfs_ftruncate first;
	 * then, make sure and update the dir_ent and file_size! */
	
	
	
	
	
	/* specially handle case where filesize was zero */
	
	
	/* be sure to update mtime with time(NULL), even for 0 bytes or ENOSPC */
	
	return b_written;
#endif
}


struct fuse_operations jg_oper = {
	.init      = jg_init,
	.destroy   = jg_destroy,
	
	.statfs    = jg_statfs,
	
	.getattr   = jg_getattr,
	.utimens   = jg_utimens,
	
	.chmod     = jg_chmod,
	.chown     = jg_chown,
	
	.fsync     = jg_fsync,
	.fsyncdir  = jg_fsyncdir,
	
	.readdir   = jg_readdir,
	.readlink  = jg_readlink,
	
	.mknod     = jg_mknod,
	.mkdir     = jg_mkdir,
	
	.unlink    = jg_unlink,
	.rmdir     = jg_rmdir,
	
	.symlink   = jg_symlink,
	.rename    = jg_rename,
	
	.open      = jg_open,
	
	.ftruncate = jg_ftruncate,
	.truncate  = jg_truncate,
	
	.read      = jg_read,
};
