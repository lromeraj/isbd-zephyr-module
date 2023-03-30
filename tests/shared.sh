#!/bin/bash

# WDIR=$(dirname $0)

function fg_process() {
  ${@:2} &
  echo $! >> $1
}

function fg_clear() {
  if [ -e $1 ]; then
    pkill -F $1
    rm -f $1
  fi
}
