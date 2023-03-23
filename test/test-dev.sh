#!/bin/bash

rm -rf twister-out*
twister  -p frdm_k64f -T . -W --device-testing --device-serial /dev/ttyACM0 --west-flash
