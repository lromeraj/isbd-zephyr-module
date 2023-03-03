#ifndef AT_UART_H_

  #define AT_UART_H_

  #include <stdint.h>
  #include <stdbool.h>

  #include "at.h"
  #include "defs.h"

  // Expected AT command response lines
  #define AT_1_LINE_RESP    1
  #define AT_2_LINE_RESP    2
  #define AT_3_LINE_RESP    3

  #define SEND_AT_CMD( fn, ... ) \
    do { \
      AT_DEFINE_CMD_BUFF( __M_at_buff ); \
      at_uart_code_t M_at_code = at_uart_write_cmd( \
        __M_at_buff, at_cmd##fn ( __M_at_buff, __VA_ARGS__ ) ); \
      if ( M_at_code != AT_UART_OK ) { return M_at_code; } \
    } while ( 0 );

  // exec
  #define SEND_AT_CMD_E( name ) \
    SEND_AT_CMD( _e, name )

  // equivalent to exec but using extra param
  #define SEND_AT_CMD_P( name, param ) \
    SEND_AT_CMD( _p, name, param )

  // test
  #define SEND_AT_CMD_T( name ) \
    SEND_AT_CMD( _t, name )

  // read
  #define SEND_AT_CMD_R( name, param ) \
    SEND_AT_CMD( _r, name, param )

  // set
  #define SEND_AT_CMD_S( name, params ) \
    SEND_AT_CMD( _s, name, params )

  typedef enum at_uart_code {
    AT_UART_TIMEOUT     = -2,
    AT_UART_RDY         = -3,
    AT_UART_ERR         = -4,
    AT_UART_UNK         = -5,
    AT_UART_OK          = 0,
  } at_uart_code_t;

  struct at_uart_config {
    bool echo;
    bool verbose;
    struct device *dev;
  };

  at_uart_code_t at_uart_setup( struct at_uart_config *at_uart_config );

  at_uart_code_t at_uart_get_str_code( const char *__buff );
  at_uart_code_t at_uart_write_cmd( char *__src_buf, size_t len );
  
  uint16_t at_uart_write( uint8_t *__src_buf, size_t len );
  uint16_t at_uart_get_n_bytes( uint8_t *bytes, uint16_t n_bytes, uint16_t timeout_ms );
  
  at_uart_code_t at_uart_check_echo();
  at_uart_code_t at_uart_pack_txt_resp( char *__str_resp, size_t str_resp_len, uint8_t lines, uint16_t timeout_ms );
  at_uart_code_t at_uart_pack_txt_resp_code( int8_t *cmd_code, uint16_t timeout_ms );
  at_uart_code_t at_uart_skip_txt_resp( uint8_t lines, uint16_t timeout_ms );

  const char *at_uart_err_to_name( at_uart_code_t code );

  int8_t at_uart_set_dtr( uint8_t option );
  int8_t at_uart_set_flow_control( uint8_t option );
  int8_t at_uart_store_active_config( uint8_t profile );
  int8_t at_uart_set_reset_profile( uint8_t profile );
  int8_t at_uart_flush_to_eeprom();
  
#endif