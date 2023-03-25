#!/bin/bash

rm -rf twister-out*
twister -vv  -p frdm_k64f -T . -W --device-testing --device-serial /dev/ttyACM0 --west-flash
# twister  -p native_posix -T . -W
