zephyr_library_named( iridium )

zephyr_library_sources(
	isbd.c
	isbd/evt.c
	isbd/msg.c
	isbd/util.c
	dte.c
	isu.c )

zephyr_include_directories( inc )
zephyr_library_link_libraries( at_uart stru kernel )
