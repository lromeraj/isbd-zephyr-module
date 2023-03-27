#!/bin/bash

source ./shared.sh
source ./clear.sh

fg_process socat -d -d pty,link=ttyS0,raw,echo=0 pty,link=ttyS1,raw,echo=0
fg_process ./zuart.py
