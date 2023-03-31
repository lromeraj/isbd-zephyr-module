#ifndef AT_UART_H_

  #define AT_UART_H_

  #include <stdint.h>
  #include <stdbool.h>

  #include "at.h"
  #include "zuart.h"

  // Expected AT command response lines
  #define AT_1_LINE_RESP    1
  #define AT_2_LINE_RESP    2
  #define AT_3_LINE_RESP    3
  
  
  #define AT_UART_RET_IF_ERR( r ) \
    if ( r != AT_UART_OK ) { return r; }

  /**
   * @brief Used to send a command and return automatically in case
   * of failure
   */
  #define AT_UART_SEND_OR_RET( ret, at_uart, at_cmd_tmpl, ... ) \
    ret = at_uart_send_cmd( at_uart, at_cmd_tmpl, __VA_ARGS__ ); \
    AT_UART_RET_IF_ERR( ret );

  typedef enum at_uart_err {
    AT_UART_OK          = 0,
    AT_UART_TIMEOUT     = -1,
    AT_UART_OVERFLOW    = -2,
    AT_UART_ERR         = -3,
    AT_UART_UNK         = -4,
  } at_uart_err_t;

  typedef struct at_uart_config {
    bool echo;
    bool verbose;
    zuart_config_t zuart;
  } at_uart_config_t;

  typedef struct at_uart {
    bool _echoed;
    unsigned char eol; // end of line char
  
    zuart_t zuart;
    at_uart_config_t config;

  } at_uart_t;

  /**
   * @brief Setup AT UART module
   * 
   * @param at_uart_config 
   * @return at_uart_code_t 
   */
  at_uart_err_t at_uart_setup( 
    at_uart_t *at_uart, struct at_uart_config *at_uart_config );

  /**
   * @brief Tries to retrieve AT command result code from the given string
   * 
   * @param str_buf String buffer to check
   * @return at_uart_code_t 
   */
  at_uart_err_t at_uart_get_str_code( 
    at_uart_t *at_uart, const char *str_buf );


  at_uart_err_t at_uart_send_cmd( 
    at_uart_t *at_uart, const char *at_cmd_tmpl, ... 
  ) __attribute__((format(printf, 2, 3)));
  
  /**
   * @brief Writes the given AT command directly to serial port
   * 
   * 
   * @param cmd AT command string buffer
   * @param cmd_len AT command string length (skipping null terminated char).
   * This is used in order to avoid recomputing string length
   * 
   * @return at_uart_code_t 
   */
  at_uart_err_t at_uart_write_cmd(
    at_uart_t *at_uart, char *cmd, size_t cmd_len );

  at_uart_err_t at_uart_check_echo( at_uart_t *at_uart );

  at_uart_err_t at_uart_pack_txt_resp( 
    at_uart_t *at_uart, char *str_resp, size_t str_resp_len, uint8_t lines, uint16_t timeout_ms );
  
  at_uart_err_t at_uart_get_cmd_resp_code( 
    at_uart_t *at_uart, int8_t *cmd_code, uint16_t timeout_ms );

  int16_t at_uart_pack_resp_code( 
    at_uart_t *at_uart, char *str_code, uint16_t str_code_len, uint16_t timeout_ms );
  
  at_uart_err_t at_uart_skip_txt_resp( 
    at_uart_t *at_uart, uint8_t lines, uint16_t timeout_ms );

  /**
   * @brief Retrieves the corresponding string from
   * the given error code
   * 
   * @param code Error code to be named
   * @return const char* A null terminated error string
   */
  const char *at_uart_err_to_name( at_uart_err_t code );

  at_uart_err_t at_uart_set_dtr( at_uart_t *at_uart, uint8_t option );
  at_uart_err_t at_uart_set_flow_control( at_uart_t *at_uart, uint8_t option );
  at_uart_err_t at_uart_store_active_config( at_uart_t *at_uart, uint8_t profile );
  at_uart_err_t at_uart_set_reset_profile( at_uart_t *at_uart, uint8_t profile );
  at_uart_err_t at_uart_flush_to_eeprom( at_uart_t *at_uart );
  
#endif