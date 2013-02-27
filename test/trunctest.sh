#!/bin/sh
# jgfs
# (c) 2013 Justin Gottula
# The source code of this project is distributed under the terms of the
# simplified BSD license. See the LICENSE file for details.


if [[ "$1" == "" ]]; then
	echo "param: number of truncations"
	exit 1
fi

for i in $(seq 1 $1); do
	truncate -s $RANDOM /mnt/0/file
done
