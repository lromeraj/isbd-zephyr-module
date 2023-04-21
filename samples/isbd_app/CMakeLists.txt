# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

# Get absolute (real path) to the module base folder
get_filename_component(
  IRIDIUM_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../ REALPATH )

# Append module path to the extra modules list
list( APPEND ZEPHYR_EXTRA_MODULES ${IRIDIUM_MODULE_PATH} )

if( "${DEBUG}" STREQUAL "yes" )
	#message( WARN "Logging mode has been enabled" )
	list( APPEND OVERLAY_CONFIG "dbg.conf" )
endif()

	set(
  	ENV{QEMU_EXTRA_FLAGS}
  	"-chardev serial,id=con1,path=$ENV{QEMU_TTY_PATH} -serial chardev:con1" )

# Look up Zephyr package using ZEPHYR_BASE
# environment variable as reference
find_package( Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE} )
project( isu_sbd_cmds )

FILE( GLOB app_sources
  src/*.c )

target_sources( app PRIVATE ${app_sources} )

# Link libraries to the target
target_link_libraries( app PRIVATE iridium )
