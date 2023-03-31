#!/bin/bash

WDIR=$(dirname $0)

source $WDIR/../shared.sh

TTY_S0_NAME="ttyS0"
TTY_S1_NAME="ttyS1"

PID_FILE_PATH="/tmp/tests.isbd.pids"

TTY_S0_PATH="${WDIR}/${TTY_S0_NAME}"
TTY_S1_PATH="${WDIR}/${TTY_S1_NAME}"

fg_clear $PID_FILE_PATH
fg_create_tty $PID_FILE_PATH $TTY_S0_PATH $TTY_S1_PATH

fg_process $PID_FILE_PATH \
  python3 $WDIR/zuart.py $TTY_S1_PATH
