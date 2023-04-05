#!/bin/bash

WDIR=$(dirname $0)

source $WDIR/../shared.sh

TTY_S1_PATH="/tmp/tests.isbd.ttyS1"
PID_FILE_PATH="/tmp/isbd.tests.pids"

ISBD_EMULATOR_PATH="${WDIR}/iridium-sbd-emulator"

# build emulator
npm --prefix $ISBD_EMULATOR_PATH run build

fg_clear $PID_FILE_PATH # clear old pids
fg_create_tty $PID_FILE_PATH $1 $TTY_S1_PATH

fg_process $PID_FILE_PATH \
  node $ISBD_EMULATOR_PATH/build/emu.js --path $TTY_S1_PATH
