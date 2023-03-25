#!/bin/bash

rm -rf twister-out*
# twister -vv  -p frdm_k64f -T . -W --device-testing --device-serial /dev/ttyACM0 --west-flash
twister -vvvvv -p qemu_cortex_m3 -T . -W
# twister -vv -p qemu_cortex_m3 -T . -W
