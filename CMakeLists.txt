# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(BOARD frdm_k64f)
# set(LIB_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/lib)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(uart_echo_bot)

# add_subdirectory(lib/)
FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources} )

add_subdirectory( lib/ )

target_link_libraries( app PRIVATE isbd )
