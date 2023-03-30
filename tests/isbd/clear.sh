#!/bin/bash

WDIR=$(dirname $0)
source $WDIR/shared.sh

if [ -e $PID_FILE_PATH ]; then
	pkill -SIGINT -F $PID_FILE_PATH
	rm -f $PID_FILE_PATH
fi
