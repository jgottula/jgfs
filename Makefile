# jgfs
# (c) 2013 Justin Gottula
# The source code of this project is distributed under the terms of the
# simplified BSD license. See the LICENSE file for details.

TIMESTAMP=$(shell date +'%Y%m%d-%H%M')

CC:=ccache gcc
CFLAGS:=-std=gnu11 -O0 -ggdb -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
LDFLAGS:=

DEFINES:=-D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26

FUSE_LIBS:=-lbsd -lfuse
MKFS_LIBS:=-lbsd
FSCK_LIBS:=-lbsd

FUSE_BIN:=bin/jgfs
MKFS_BIN:=bin/mkjgfs
FSCK_BIN:=bin/jgfsck

OBJS=$(patsubst %.c,%.o,$(wildcard src/*/*.c))
COMMON_OBJS=$(filter src/common/%.o,$(OBJS))


.PHONY: all clean backup fuse mkfs fsck

# default rule
all: fuse mkfs fsck

fuse: $(FUSE_BIN)
mkfs: $(MKFS_BIN)
fsck: $(FSCK_BIN)

clean:
	rm -rf $(wildcard bin/*) $(wildcard src/*/*.o) $(wildcard src/*/*.dep)

backup:
	cd .. && tar -acvf backup/jgfs-$(TIMESTAMP).tar.xz jgfs/


$(FUSE_BIN): $(filter src/fuse/%.o,$(OBJS)) $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(FUSE_LIBS) -o $@ $^
$(MKFS_BIN): $(filter src/mkfs/%.o,$(OBJS)) $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(MKFS_LIBS) -o $@ $^
$(FSCK_BIN): $(filter src/fsck/%.o,$(OBJS)) $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(FSCK_LIBS) -o $@ $^


%.o: %.c Makefile
	$(CC) $(CFLAGS) $(DEFINES) -o $@ -Isrc -MP -MMD -MF $(<D)/$(patsubst %.c,%.dep,$(<F)) -c $<


# concatenate gcc's autogenerated dependencies
-include src/*/*.dep
