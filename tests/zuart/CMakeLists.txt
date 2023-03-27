# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

if(TEST)

  # This flags will be passed to QEMU which in this case 
  # are used to create a virtual PTY in order to communicate
  # with the virtual device
  set(
    ENV{QEMU_EXTRA_FLAGS}
    "-chardev serial,id=con1,path=${CMAKE_CURRENT_SOURCE_DIR}/ttyS0 -serial chardev:con1" )

  execute_process( COMMAND bash setup.sh
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