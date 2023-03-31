#!/bin/bash

# WDIR=$(dirname $0)

function wait_for_str {
  tail -n0 $1 | grep -qi $2
}

function fg_process {
  out="/tmp/`basename $2`_${RANDOM}${RANDOM}.out"
  ${@:2} &> $out &
  echo $! >> $1
  echo $out
}

function fg_clear {
  if [ -e $1 ]; then
    pkill -F $1
    rm -f $1
  fi
}

function fg_create_tty {
  file=$(fg_process $1 \
    socat -dd pty,link=$2,raw,echo=0 pty,link=$3,raw,echo=0)

  wait_for_str $file "starting"
}