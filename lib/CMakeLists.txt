add_subdirectory( zuart )
add_subdirectory( at_uart )
add_subdirectory( stru )
add_subdirectory( iridium )

# # str-ing u-tils library
# add_library( stru STATIC
# 	stru/stru.c )

# target_include_directories( stru PUBLIC stru/inc )

# # Zephyr UART library
# add_library( zuart STATIC
# 	zuart/zuart.c )

# target_link_libraries( zuart zephyr_interface )
# target_include_directories( zuart PUBLIC zuart/inc )

# # AT UART libraray
# add_library( at_uart STATIC
# 	at_uart/at.c
# 	at_uart/at_uart.c )

# target_link_libraries( at_uart zuart stru )
# target_include_directories( at_uart PUBLIC at_uart/inc )

# # Iridium SBD library
# add_library( isbd STATIC
# 	isbd/isbd.c )

# target_link_libraries( isbd at_uart stru )
# target_include_directories( isbd PUBLIC isbd/inc )
