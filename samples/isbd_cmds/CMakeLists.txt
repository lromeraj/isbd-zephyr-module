# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

get_filename_component(
  IRIDIUM_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../ REALPATH )

list( APPEND ZEPHYR_EXTRA_MODULES ${IRIDIUM_MODULE_PATH} )


	set(
  	ENV{QEMU_EXTRA_FLAGS}
  	"-chardev serial,id=con1,path=$ENV{QEMU_TTY_PATH} -serial chardev:con1" )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(isu_sbd_cmds)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources} )

target_link_libraries( app PRIVATE iridium )
