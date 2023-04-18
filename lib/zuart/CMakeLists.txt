if(CONFIG_Z_UART)

  zephyr_library_named( zuart )

  zephyr_library_sources( zuart.c )

  zephyr_include_directories( inc/ )
  zephyr_library_link_libraries( zephyr )
  
endif()
