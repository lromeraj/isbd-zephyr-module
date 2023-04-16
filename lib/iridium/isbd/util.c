#include "isbd/util.h"

uint16_t isbd_util_compute_checksum( const uint8_t *msg_buf, uint16_t msg_buf_len ) {
  uint32_t sum = 0;
  for ( uint16_t i = 0; i < msg_buf_len; i++ ) {
    sum += msg_buf[ i ];
  }  
  return (sum & 0xFFFF);
}
