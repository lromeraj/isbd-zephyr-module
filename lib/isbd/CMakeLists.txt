zephyr_library_named(isbd)
zephyr_library_sources(isbd.c dte.c evt.c isu.c)
zephyr_include_directories(inc)
zephyr_library_link_libraries(at_uart stru)