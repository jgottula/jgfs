#ifndef JGFS_FUSE_FS_H
#define JGFS_FUSE_FS_H


#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION  26
#include <fuse.h>


int jgfs_getattr(const char *path, struct stat *buf);
int jgfs_mknod(const char *path, mode_t mode, dev_t dev);
int jgfs_mkdir(const char *path, mode_t mode);
int jgfs_unlink(const char *path);
int jgfs_rmdir(const char *path);
int jgfs_symlink(const char *path, const char *link);
int jgfs_rename(const char *path, const char *newpath);
int jgfs_truncate(const char *path, off_t newsize);
int jgfs_readlink(const char *path, char *link, size_t size);
int jgfs_open(const char *path, struct fuse_file_info *fi);
int jgfs_read(const char *path, char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi);
int jgfs_write(const char *path, const char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi);
int jgfs_statfs(const char *path, struct statvfs *statv);
int jgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi);
void *jgfs_init(struct fuse_conn_info *conn);
void jgfs_destroy(void *userdata);
int jgfs_ftruncate(const char *path, off_t newsize, struct fuse_file_info *fi);
int jgfs_utimens(const char *path, const struct timespec tv[2]);


#endif
