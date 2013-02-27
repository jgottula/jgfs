#!/bin/sh
# jgfs
# (c) 2013 Justin Gottula
# The source code of this project is distributed under the terms of the
# simplified BSD license. See the LICENSE file for details.


make || exit $?

rm -f test/flop.img
dd if=/dev/zero of=test/flop.img bs=512 count=2880

bin/mkjgfs test/flop.img || exit $?

sudo umount /mnt/0
bin/jgfs test/flop.img /mnt/0
