#!/bin/bash

# export QEMU_EXTRA_FLAGS="-chardev pty,id=con1,mux=on -serial chardev:con1"

rm -rf twister-out*
twister -vv -p qemu_cortex_m3 -T . -W

# twister -vv -p qemu_cortex_m3 -T . -W
# twister -vv  -p frdm_k64f -T . -W --device-testing --device-serial /dev/ttyACM0 --west-flash
