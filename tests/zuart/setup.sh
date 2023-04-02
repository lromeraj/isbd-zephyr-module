#!/bin/bash

WDIR=$(dirname $0)

source $WDIR/../shared.sh

PID_FILE_PATH="/tmp/tests.zuart.pids"

# TTY_S0_PATH="/tmp/${BASE_NAME}.${TTY_S0_NAME}"
TTY_S1_PATH="/tmp/tests.zuart.ttyS1"

fg_clear $PID_FILE_PATH
fg_create_tty $PID_FILE_PATH $1 $TTY_S1_PATH

fg_process $PID_FILE_PATH \
  python3 $WDIR/zuart.py $TTY_S1_PATH
