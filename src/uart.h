#ifndef AT_UART_H_

  #define AT_UART_H_

  #include <stdint.h>
  #include <stdbool.h>

  #include "at.h"

  // Expected AT command response lines
  #define AT_1_LINE_RESP    1
  #define AT_2_LINE_RESP    2
  #define AT_3_LINE_RESP    3

  /**
   * @brief Used to send a command and return automatically in case
   * of failure
   * 
   * @note This macro defines a temporary buffer to store the AT
   * command string, finally result code is used in order to return
   * automatically or not
   */
  #define SEND_AT_CMD_OR_RET( fn, ... ) \
    do { \
      AT_DEFINE_CMD_BUFF( _M_at_buff ); \
      at_uart_code_t _M_at_code = at_uart_write_cmd( \
        _M_at_buff, at_cmd##fn ( _M_at_buff, __VA_ARGS__ ) ); \
      if ( _M_at_code != AT_UART_OK ) { return _M_at_code; } \
    } while ( 0 );

  /**
   * @brief Exec AT command, returns in case of failure
   * @param name AT command name
   */
  #define SEND_AT_CMD_E_OR_RET( name ) \
    SEND_AT_CMD_OR_RET( _e, name )

  /**
   * @brief Exec AT command with parameter in place, returns in case of failure
   * @param name AT command name
   * @param param Unsigned byte param, this number will be concatenated to the AT command name
   */
  #define SEND_AT_CMD_P_OR_RET( name, param ) \
    SEND_AT_CMD_OR_RET( _p, name, param )

  /**
   * @brief Test AT command, returns in case of failure
   * @param name AT command name
   */
  #define SEND_AT_CMD_T_OR_RET( name ) \
    SEND_AT_CMD_OR_RET( _t, name )

  /**
   * @brief Read AT command, returns in case of failure
   * @param name AT command name
   */
  #define SEND_AT_CMD_R_OR_RET( name ) \
    SEND_AT_CMD_OR_RET( _r, name )

  /**
   * @brief Set AT command, returns in case of failure
   * @param name AT command name
   * @param params A string which contains command parameters
   */
  #define SEND_AT_CMD_S_OR_RET( name, params ) \
    SEND_AT_CMD_OR_RET( _s, name, params )

  
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


  /**
   * @brief Setup AT UART module
   * 
   * @param at_uart_config 
   * @return at_uart_code_t 
   */
  at_uart_code_t at_uart_setup( struct at_uart_config *at_uart_config );

  /**
   * @brief Tries to retrieve AT command result code from the given string
   * 
   * @param str_buf String buffer to check
   * @return at_uart_code_t 
   */
  at_uart_code_t at_uart_get_str_code( const char *str_buf );

  /**
   * @brief Writes the given AT command directly to serial port
   * 
   * @note This function should be used while sending AT commands. 
   * In order to simplify AT command build process
   * you can use macros like: SEND_AT_CMD_<TYPE>_OR_RET.
   * 
   * @param cmd AT command string buffer
   * @param cmd_len AT command string length (skipping null terminated char).
   * This is used in order to avoid recomputing string length, useful when using
   *  SEND_AT_CMD_<TYPE>_OR_RET like macros
   * 
   * @return at_uart_code_t 
   */
  at_uart_code_t at_uart_write_cmd( char *cmd, size_t cmd_len );
  
  /**
   * @brief Writes the given buffer directly to serial port
   * 
   * @param buf Buffer to be written
   * @param buf_len Buffer length
   * @return uint16_t 
   */
  uint16_t at_uart_write( uint8_t *buf, size_t buf_len );
  uint16_t at_uart_get_n_bytes( uint8_t *bytes, uint16_t n_bytes, uint16_t timeout_ms );
  
  at_uart_code_t at_uart_check_echo();
  at_uart_code_t at_uart_pack_txt_resp( char *str_resp, size_t str_resp_len, uint8_t lines, uint16_t timeout_ms );
  at_uart_code_t at_uart_pack_txt_resp_code( int8_t *cmd_code, uint16_t timeout_ms );
  at_uart_code_t at_uart_skip_txt_resp( uint8_t lines, uint16_t timeout_ms );

  /**
   * @brief Retrieves the corresponding string from
   * the given error code
   * 
   * @param code Error code to be named
   * @return const char* A null terminated error string
   */
  const char *at_uart_err_to_name( at_uart_code_t code );

  int8_t at_uart_set_dtr( uint8_t option );
  int8_t at_uart_set_flow_control( uint8_t option );
  int8_t at_uart_store_active_config( uint8_t profile );
  int8_t at_uart_set_reset_profile( uint8_t profile );
  int8_t at_uart_flush_to_eeprom();
  
#endif