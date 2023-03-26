# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(BOARD frdm_k64f)

# TODO: move this to other size
if(TEST)

  # This flags will be passed to QEMU which in this case 
  # are used to create a virtual PTY in order to communicate
  # with the virtual device
	set(
		ENV{QEMU_EXTRA_FLAGS}
		"-chardev pty,id=con1,mux=on -serial chardev:con1" )

  # This script will be automatically killed 
  # when CMake process exits
  execute_process( COMMAND bash -c "./zuart.py &"
    INPUT_FILE /dev/null
    OUTPUT_FILE /dev/null
    ERROR_FILE /dev/null
    RESULT_VARIABLE RESULT )
    
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(isbd_test)

target_sources(app PRIVATE
	src/test_zuart.c)

add_subdirectory( ../../lib ${CMAKE_CURRENT_BINARY_DIR}/lib )

target_link_libraries( app PRIVATE zuart )

# ../boards/arm/qemu_cortex_m3/qemu_cortex_m3.dts
