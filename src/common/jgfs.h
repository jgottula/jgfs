#ifndef JGFS_COMMON_JGFS_H
#define JGFS_COMMON_JGFS_H


/* this header must be located at sector 1 (offset 0x200~0x400) */
struct jgfs_header {
	char     magic[4];  // must be "JGFS"
	uint8_t  ver_major; // major version
	uint8_t  ver_minor; // minor version
	
	uint16_t sz_total;  // total number of sectors
	
	uint16_t sz_rsvd;   // sectors reserved for vbr + header + boot area
	uint16_t sz_fat;    // sectors reserved for the fat
	
	char     reserved[0x1f4];
};

typedef uint16_t fat_ent_t;

struct jgfs_fat_sector {
	fat_ent_t entries[0x100];
};

enum jgfs_fat_entry {
	FAT_FREE  = 0x0000, // free / root directory
	
	FAT_FIRST = 0x0001, // first normal cluster
	/* ... */
	FAT_LAST  = 0xfffb, // last possible normal cluster
	
	/* special */
	FAT_EOF   = 0xfffc, // last cluster in file
	FAT_RSVD  = 0xfffd, // reserved
	FAT_BAD   = 0xfffe, // damaged
	FAT_OOB   = 0xffff, // past end of device
};

struct jgfs_dir_entry {
	char      name[20]; // [A-Za-z0-9_.] zero-pad; 19ch max; zero = unused entry
	uint32_t  mtime;    // unix time
	uint16_t  attrib;   // file/dir attributes
	fat_ent_t begin;    // first cluster of file/dir (0 for empty file)
	uint32_t  size;     // file size in bytes
};

enum jgfs_file_attrib {
	FILE_DIR = (1 << 0), // is a directory
};

struct jgfs_dir_cluster {
	fat_ent_t me;         // first cluster of this dir
	fat_ent_t parent;     // first cluster of parent dir
	
	char      reserved[28];
	
	struct jgfs_dir_entry entries[15];
};


_Static_assert(sizeof(struct jgfs_header) == 0x200,
	"jgfs_header must be 512 bytes");
_Static_assert(sizeof(struct jgfs_fat_sector) == 0x200,
	"jgfs_fat_sector must be 512 bytes");
_Static_assert(sizeof(struct jgfs_dir_entry) == 32,
	"jgfs_dir_entry must be 32 bytes");


#endif
