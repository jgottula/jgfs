#ifndef JGFS_COMMON_VERSION_H
#define JGFS_COMMON_VERSION_H


#define strify_ver(s) _strify_ver(s)
#define _strify_ver(s) #s


#define JGFS_VERSION     0x0000


const char *argp_program_version = "jgfs " strify_ver(JGFS_VERSION);


#undef _strify_ver
#undef strify_ver


#endif
