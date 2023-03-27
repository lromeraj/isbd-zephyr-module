#!/bin/bash

if [ -e pids ]; then
	kill `cat pids`
	rm -f pids
fi
