#ifndef JGFS_COMMON_JGFS_H
#define JGFS_COMMON_JGFS_H


/* this header must be located at sector 7 (offset 0x0e00-0x1000) */
struct jgfs_header {
	char     magic[4]; // must be "JGFS"
	uint16_t version;  // msb is major, lsb is minor
	
	uint16_t s_per_c;  // sectors per cluster [1,2,4,8]
	uint16_t nr_c;     // number of clusters  [at least ___]
	
	char     reserved[0x1f6];
};

_Static_assert(sizeof(struct jgfs_header) == 0x200,
	"jgfs_header must be 512 bytes");


#endif
