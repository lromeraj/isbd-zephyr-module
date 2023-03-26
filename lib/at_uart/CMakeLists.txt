zephyr_library_named(at_uart)
zephyr_library_sources(at.c at_uart.c)
zephyr_include_directories(inc)
zephyr_library_link_libraries(zuart stru)
