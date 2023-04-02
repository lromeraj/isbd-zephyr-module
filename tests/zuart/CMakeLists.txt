# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

if(QEMU_PIPE)

  # This flags will be passed to QEMU which in this case
  # are used to create a virtual PTY in order to communicate
  # with the virtual device

  set(QEMU_TTY_PATH "/tmp/tests.zuart.ttyS0")
  set(TEST_SETUP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/setup.sh")

  set(
    ENV{QEMU_EXTRA_FLAGS}
    "-chardev serial,id=con1,path=${QEMU_TTY_PATH} -serial chardev:con1" )

  execute_process( COMMAND bash ${TEST_SETUP_PATH} ${QEMU_TTY_PATH}
    INPUT_FILE /dev/null
    OUTPUT_FILE /dev/null
    ERROR_FILE /dev/null )

endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(isbd_test)

target_sources(app PRIVATE
	src/test_zuart.c)

add_subdirectory( ../../lib ${CMAKE_CURRENT_BINARY_DIR}/lib )

target_link_libraries( app PUBLIC zuart )

# ../boards/arm/qemu_cortex_m3/qemu_cortex_m3.dts
