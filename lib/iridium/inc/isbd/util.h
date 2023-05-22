#ifndef ISBD_UTIL_H_
  #define ISBD_UTIL_H_

  #include <stdint.h>

  /**
   * @brief Computes the checksum from the given message buffer
   *
   * @param msg_buf Message buffer
   * @param msg_buf_len Message buffer length
   * @return uint16_t Message checksum
   */
  uint16_t isbd_util_compute_checksum( const uint8_t *msg_buf, uint16_t msg_buf_len );

#endif
