#ifndef AT_UART_H_
  #define AT_UART_H_

  #include <stdint.h>
  #include <stdbool.h>

  #include "defs.h"

  // Expected AT command response lines
  #define AT_1_LINE_RESP    1
  #define AT_2_LINE_RESP    2
  #define AT_3_LINE_RESP    3

  typedef enum at_uart_code {
    ISBD_AT_RING        = -1,
    ISBD_AT_TIMEOUT     = -2,
    ISBD_AT_RDY         = -3,
    ISBD_AT_ERR         = -4,
    ISBD_AT_UNK         = -5,
    ISBD_AT_OK          = 0,
  } at_uart_code_t;

  struct at_uart_config {
    bool echo;
    bool verbose;
    struct device *dev;
  };

  isbd_err_t at_uart_setup();

  at_uart_code_t at_uart_get_str_code( const char *__buff );
  uint16_t at_uart_write( uint8_t *__src_buf, uint16_t len );
  at_uart_code_t at_uart_write_cmd( uint8_t *__src_buf, uint16_t len );
  uint16_t at_uart_get_n_bytes( uint8_t *bytes, uint16_t n_bytes, uint16_t timeout_ms );
  
  at_uart_code_t at_uart_check_echo();
  at_uart_code_t at_uart_pack_txt_resp( char *__str_resp, uint16_t str_resp_len, uint8_t lines, uint16_t timeout_ms );
  at_uart_code_t at_uart_pack_txt_resp_code( int8_t *cmd_code, uint16_t timeout_ms );
  at_uart_code_t at_uart_skip_txt_resp( uint8_t lines, uint16_t timeout_ms );

#endif