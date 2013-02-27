#!/bin/sh
# jgfs
# (c) 2013 Justin Gottula
# The source code of this project is distributed under the terms of the
# simplified BSD license. See the LICENSE file for details.


PID=$(pidof jgfs)

if [[ "$PID" == "" ]]; then
  echo "jgfs is not running"
  exit 1
fi

gdb bin/jgfs $PID
