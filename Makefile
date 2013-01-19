# jgfs
# (c) 2013 Justin Gottula
# The source code of this project is distributed under the terms of the
# simplified BSD license. See the LICENSE file for details.

CC:=x86_64-unknown-linux-gnu-gcc-4.8.0
CFLAGS:=-std=gnu11 -Og -ggdb -Wall -Wextra -flto
LDFLAGS:=
LIBS:=

FUSE_BIN:=bin/jgfs
MKFS_BIN:=bin/mkjgfs
FSCK_BIN:=bin/jgfsck

# resolved when referenced
OBJS=$(patsubst %.c,%.o,$(wildcard src/*/*.c))


.PHONY: all clean fuse mkfs fsck

# default rule
all: fuse mkfs fsck

fuse: $(FUSE_BIN)
mkfs: $(MKFS_BIN)
fsck: $(FSCK_BIN)

clean:
	rm -rf $(wildcard bin/*) $(wildcard src/*/*.o) $(wildcard src/*/*.dep)


$(FUSE_BIN): $(filter src/fuse/%.o,$(OBJS))
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^
$(MKFS_BIN): $(filter src/mkfs/%.o,$(OBJS))
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^
$(FSCK_BIN): $(filter src/fsck/%.o,$(OBJS))
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^


%.o: %.c Makefile
	$(CC) $(CFLAGS) -o $@ -Isrc -MP -MMD -MF $(<D)/$(patsubst %.c,%.dep,$(<F)) -c $<


# concatenate gcc's autogenerated dependencies
-include src/*/*.dep
