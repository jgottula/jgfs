jgfs
====
`jgfs` is a simple filesystem, written from scratch to imitate the overall
design of `FAT16` while excluding all of the `DOS` legacy nonsense that normally
comes with a `FAT` filesystem. Its primary intended purpose is for use with the
`justix` operating system project.

status
------
`jgfs` is no longer being actively developed, as `jgfs2` is now the preferred
filesystem of the `justix` operating system. The filesystem code itself is
mostly stable, but hasn't been tested on a large scale.

building
--------
This project uses Avery Pennarun's implementation of D. J. Bernstein's `redo`
build system. For more information, see [djb's redo page][1] and [apenwarr's
redo implementation][2].

To build `libjgfs`, the `FUSE` program, and associated utilities, install `redo`
and simply run `redo all`. To clean the project directory, run `redo clean`.

To build individual targets, run one of the following:

- `redo lib`: build the `libjgfs` library, `bin/libjgfs.a`
- `redo fuse`: build the `FUSE` program, `bin/jgfs`
- `redo mkfs`: build the `mkfs` utility, `bin/mkjgfs`
- `redo fsck`: build the `fsck` utility, `bin/jgfsck`

running
-------
Make a new `jgfs` filesystem on a device or file:

    bin/mkjgfs <device>

Mount the filesystem using `FUSE`:

    bin/jgfs <device> <mountpoint>

directories
-----------
- `bin`: contains the `libjgfs` library and utility binaries after a build
- `doc`: documentation related to the filesystem's development
- `lib`: source code for `libjgfs`
- `src`: source code for `jgfs` utilities and the `FUSE` program
- `test`: various scripts used for testing the filesystem

license
-------
This project is licensed under the terms of the simplified (2-clause) BSD
license. For more information, see the `LICENSE` file contained in the project's
root directory.


[1]: http://cr.yp.to/redo.html
[2]: https://github.com/apenwarr/redo
