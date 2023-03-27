#!/bin/bash

function fg_process() {
	$@ &
	echo $! >> pids
}