# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# This should be removed in a near future
set(BOARD frdm_k64f)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(uart_echo_bot)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources} )

add_subdirectory( lib/ )

target_link_libraries( app PRIVATE isbd )
