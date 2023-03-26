# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(BOARD frdm_k64f)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(isbd_test)

target_sources(app PRIVATE
	src/test_zuart.c)

add_subdirectory( ../../lib ${CMAKE_CURRENT_BINARY_DIR}/lib )

target_link_libraries( app PRIVATE zuart )

# ../boards/arm/qemu_cortex_m3/qemu_cortex_m3.dts