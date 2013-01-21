#!/bin/sh

PID=$(pidof jgfs)

if [[ "$PID" == "" ]]; then
  echo "jgfs is not running"
  exit 1
fi

gdb bin/jgfs $PID
