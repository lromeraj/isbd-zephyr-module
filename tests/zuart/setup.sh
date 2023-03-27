#!/bin/bash


WDIR=$(dirname $0)

source $WDIR/shared.sh
source $WDIR/clear.sh

TTY_S0_NAME="ttyS0"
TTY_S1_NAME="ttyS1"

TTY_S0_PATH="${WDIR}/${TTY_S0_NAME}"
TTY_S1_PATH="${WDIR}/${TTY_S1_NAME}"

fg_process socat -d -d pty,link=$TTY_S0_PATH,raw,echo=0 pty,link=$TTY_S1_PATH,raw,echo=0
fg_process python3 $WDIR/zuart.py $TTY_S1_PATH
