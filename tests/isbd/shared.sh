#!/bin/bash

WDIR=$(dirname $0)
PID_FILE_PATH=$WDIR/pids

function fg_process() {
	$@ &
	echo $! >> $PID_FILE_PATH
}
