#define DEBUG_BYTE( byte ) \
  if ( byte == '\r' || byte == '\n' ) { \
    printk( "%d", byte ); \
  } else { \
    printk( "%c", byte ); \
  }