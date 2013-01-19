#ifndef JGFS_FUSE_FS_H
#define JGFS_FUSE_FS_H


#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION  26
#include <fuse.h>


int jgfs_getattr(const char *path, struct stat *buf);
int jgfs_readlink(const char *path, char *link, size_t size);
int jgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi);
void *jgfs_init(struct fuse_conn_info *conn);
void jgfs_destroy(void *userdata);


#endif
