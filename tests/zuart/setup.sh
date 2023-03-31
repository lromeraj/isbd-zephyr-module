#!/bin/bash

WDIR=$(dirname $0)

source $WDIR/../shared.sh

TTY_S0_NAME="ttyS0"
TTY_S1_NAME="ttyS1"

PID_FILE_PATH="${WDIR}/pids"

TTY_S0_PATH="${WDIR}/${TTY_S0_NAME}"
TTY_S1_PATH="${WDIR}/${TTY_S1_NAME}"

fg_clear $PID_FILE_PATH

fg_process $PID_FILE_PATH \
  socat -d -d pty,link=$TTY_S0_PATH,raw,echo=0 pty,link=$TTY_S1_PATH,raw,echo=0

fg_process $PID_FILE_PATH \
  python3 $WDIR/zuart.py $TTY_S1_PATH