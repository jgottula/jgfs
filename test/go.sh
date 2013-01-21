#!/bin/sh

make || exit $?

rm -f test/flop
dd if=/dev/zero of=test/flop bs=512 count=2880

bin/mkjgfs test/flop || exit $?

sudo umount /mnt/0
bin/jgfs test/flop /mnt/0
