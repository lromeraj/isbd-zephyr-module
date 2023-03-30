#!/bin/bash

WDIR=$(dirname $0)

source $WDIR/../shared.sh

PID_FILE_PATH="${WDIR}/pids"

TTY_S0_NAME="ttyS0"
TTY_S1_NAME="ttyS1"

TTY_S0_PATH="${WDIR}/${TTY_S0_NAME}"
TTY_S1_PATH="${WDIR}/${TTY_S1_NAME}"

ISBD_EMULATOR_PATH="${WDIR}/iridium-sbd-emulator"

# build emulator
npm --prefix $ISBD_EMULATOR_PATH run build

fg_clear $PID_FILE_PATH # clear old pids

fg_process $PID_FILE_PATH \
  socat -dd pty,link=$TTY_S0_PATH,raw,echo=0 pty,link=$TTY_S1_PATH,raw,echo=0

fg_process $PID_FILE_PATH \
  node $ISBD_EMULATOR_PATH/build/isbdemu.js --path $TTY_S1_PATH
