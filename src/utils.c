#include <stdbool.h>

bool streq( const char *__src, const char *__ref ) {
  while ( *__src && *__ref ) {
    if ( *__src != *__ref ) {
      return false;
    }
    __src++; 
    __ref++;
  }
  return *__src == *__ref;
}
