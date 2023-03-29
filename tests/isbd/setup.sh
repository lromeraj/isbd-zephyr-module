#!/bin/bash

WDIR=$(dirname $0)

source $WDIR/shared.sh
source $WDIR/clear.sh

TTY_S0_NAME="ttyS0"
TTY_S1_NAME="ttyS1"

TTY_S0_PATH="${WDIR}/${TTY_S0_NAME}"
TTY_S1_PATH="${WDIR}/${TTY_S1_NAME}"

ISBD_EMULATOR_PATH="${WDIR}/iridium-sbd-emulator"

npm --prefix $ISBD_EMULATOR_PATH run build

echo "YUPI $TTY_S0_PATH $TTY_S1_PATH"

fg_process socat -dd pty,link=$TTY_S0_PATH,raw,echo=0 pty,link=$TTY_S1_PATH,raw,echo=0
fg_process node $ISBD_EMULATOR_PATH/build/isbdemu.js --path $TTY_S1_PATH
