#ifndef JGFS_COMMON_JGFS_H
#define JGFS_COMMON_JGFS_H


#include "macro.h"
#include <stdbool.h>
#include <stdint.h>


#define SECT_SIZE 0x200

#define JGFS_VER_MAJOR 0x02
#define JGFS_VER_MINOR 0x01

#define JGFS_MAGIC "JGFS"

#define JGFS_HDR_SECT 1

#define JGFS_FENT_PER_S \
	(SECT_SIZE / sizeof(fat_ent_t))

#define JGFS_DENT_PER_C \
	((jgfs_clust_size() / sizeof(struct jgfs_dir_ent)) - 1)

#define JGFS_VER_EXPAND(_maj, _min) \
	(((uint16_t)_maj * 0x100) + (uint16_t)_min)


struct sect {
	uint8_t data[SECT_SIZE];
};

typedef uint16_t fat_ent_t;

struct jgfs_fat_sect {
	fat_ent_t entries[JGFS_FENT_PER_S];
};

enum jgfs_fat_val {
	FAT_FREE  = 0x0000, // free
	FAT_ROOT  = 0x0000, // root directory
	
	FAT_FIRST = 0x0001, // first normal cluster
	/* ... */
	FAT_LAST  = 0xfffb, // last possible normal cluster
	
	/* special */
	FAT_EOF   = 0xfffc, // last cluster in file
	FAT_RSVD  = 0xfffd, // reserved
	FAT_BAD   = 0xfffe, // damaged
	FAT_OOB   = 0xffff, // past end of device
};

struct  __attribute__((__packed__)) jgfs_dir_ent {
	char      name[20]; // [A-Za-z0-9_.] zero-pad; 19ch max; zero = unused entry
	uint8_t   type;     // type (mutually exclusive)
	uint8_t   attr;     // attributes (bitmask)
	uint32_t  mtime;    // unix time
	uint32_t  size;     // size in bytes
	fat_ent_t begin;    // first cluster (0 for empty file)
};

enum jgfs_file_type {
	TYPE_FILE    = (1 << 0), // regular file
	TYPE_DIR     = (1 << 1), // directory
	TYPE_SYMLINK = (1 << 2), // symlink
};

enum jgfs_file_attr {
	ATTR_NONE = 0,
	
	/* ... */
};

struct __attribute__((__packed__)) jgfs_dir_clust {
	fat_ent_t me;         // first cluster of this dir
	fat_ent_t parent;     // first cluster of parent dir
	
	char      reserved[28];
	
	struct jgfs_dir_ent entries[0];
};

/* this header must be located at sector 1 (offset 0x200~0x400) */
struct __attribute__((__packed__)) jgfs_hdr {
	char     magic[4];  // must be "JGFS"
	uint8_t  ver_major; // major version
	uint8_t  ver_minor; // minor version
	
	uint32_t s_total;   // total number of sectors
	
	uint16_t s_rsvd;    // sectors reserved for vbr + header + boot area
	uint16_t s_fat;     // sectors reserved for the fat
	
	uint16_t s_per_c;   // sectors per cluster
	
	struct jgfs_dir_ent root_dir_ent; // root directory entry
	
	char     reserved[0x1d0];
};


_Static_assert(sizeof(struct sect) == 0x200,
	"sect must be 512 bytes");
_Static_assert(sizeof(struct jgfs_hdr) == 0x200,
	"jgfs_hdr must be 512 bytes");
_Static_assert(sizeof(struct jgfs_fat_sect) == 0x200,
	"jgfs_fat_sect must be 512 bytes");
_Static_assert(sizeof(struct jgfs_dir_ent) == 32,
	"jgfs_dir_ent must be 32 bytes");
_Static_assert(sizeof(struct jgfs_dir_clust) == 32,
	"jgfs_dir_clust must be 32 bytes");


void jgfs_init(const char *dev_path);
void jgfs_new(const char *dev_path,
	uint32_t s_total, uint16_t s_rsvd, uint16_t s_per_c);
void jgfs_done(void);
void jgfs_sync(void);
uint32_t jgfs_clust_size(void);
void *jgfs_get_sect(uint32_t sect_num);
void *jgfs_get_clust(fat_ent_t clust_num);
fat_ent_t jgfs_fat_read(fat_ent_t addr);
void jgfs_fat_write(fat_ent_t addr, fat_ent_t val);
bool jgfs_find_free_clust(fat_ent_t *dest);


extern struct jgfs_hdr      *hdr;
extern struct sect          *rsvd;
extern struct jgfs_fat_sect *fat;


#endif
