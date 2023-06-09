# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

# Get absolute (real path) to the module base folder
get_filename_component(
  IRIDIUM_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../ REALPATH )

# Append module path to the extra modules list
list( APPEND ZEPHYR_EXTRA_MODULES ${IRIDIUM_MODULE_PATH} )

if("${DEBUG}" STREQUAL "y")
	message( INFO "Debug mode has been enabled" )
	list( APPEND OVERLAY_CONFIG "dbg.conf" )
endif()

if(DEFINED QEMU_TTY_PATH)
  set(
    ENV{QEMU_EXTRA_FLAGS}
    "-chardev serial,id=con1,path=${QEMU_TTY_PATH} -serial chardev:con1" )
endif()

# Look up Zephyr package using ZEPHYR_BASE
# environment variable as reference
find_package( Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE} )
project( isu_sbd_app )

# FILE( GLOB app_sources src/*.c )

target_sources( app PRIVATE 
  src/main.c 
  src/rgb.c )

# Link libraries to the target
target_link_libraries( app PRIVATE iridium )
