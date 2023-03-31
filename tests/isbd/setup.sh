#!/bin/bash

WDIR=$(dirname $0)

source $WDIR/../shared.sh

PID_FILE_PATH="/tmp/isbd.tests.pids"

TTY_S0_NAME="ttyS0"
TTY_S1_NAME="ttyS1"

TTY_S0_PATH="${WDIR}/${TTY_S0_NAME}"
TTY_S1_PATH="${WDIR}/${TTY_S1_NAME}"

ISBD_EMULATOR_PATH="${WDIR}/iridium-sbd-emulator"

# build emulator
npm --prefix $ISBD_EMULATOR_PATH run build

fg_clear $PID_FILE_PATH # clear old pids

fg_create_tty $PID_FILE_PATH $TTY_S0_PATH $TTY_S1_PATH

fg_process $PID_FILE_PATH \
  node $ISBD_EMULATOR_PATH/build/isbdemu.js --path $TTY_S1_PATH
