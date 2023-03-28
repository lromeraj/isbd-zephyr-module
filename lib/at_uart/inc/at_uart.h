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

  /**
   * @brief Used to send a command and return automatically in case
   * of failure
   * 
   * @note This macro defines a temporary buffer to store the AT
   * command string, finally the result code is used in order to return
   * automatically or not
   * TODO: we should have an argumento for the command buffer
   */
  #define SEND_AT_CMD_OR_RET( at_uart, fn, ... ) \
    do { \
      AT_DEFINE_CMD_BUFF( _M_at_buff ); \
      at_uart_err_t _M_at_code = at_uart_write_cmd( \
        at_uart, _M_at_buff, at_cmd##fn ( _M_at_buff, __VA_ARGS__ ) ); \
      if ( _M_at_code != AT_UART_OK ) { return _M_at_code; } \
    } while ( 0 );

  /**
   * @brief Exec AT command, returns in case of failure
   * @param name AT command name
   */
  #define SEND_AT_CMD_E_OR_RET( at_uart, name ) \
    SEND_AT_CMD_OR_RET( at_uart, _e, name )

  /**
   * @brief Exec AT command with parameter in place, returns in case of failure
   * @param name AT command name
   * @param param Unsigned byte param, this number will be concatenated to the AT command name
   */
  #define SEND_AT_CMD_P_OR_RET( at_uart, name, param ) \
    SEND_AT_CMD_OR_RET( at_uart, _p, name, param )

  /**
   * @brief Exec AT command with a string parameter in place, returns in case of failure
   * @param name AT command name
   * @param param String param, this string will be concatenated with the AT command name
   */
  #define SEND_AT_CMD_PS_OR_RET( at_uart, name, param ) \
    SEND_AT_CMD_OR_RET( at_uart, _ps, name, param )

  /**
   * @brief Test AT command, returns in case of failure
   * @param name AT command name
   */
  #define SEND_AT_CMD_T_OR_RET( at_uart, name ) \
    SEND_AT_CMD_OR_RET( at_uart, _t, name )

  /**
   * @brief Read AT command, returns in case of failure
   * @param name AT command name
   */
  #define SEND_AT_CMD_R_OR_RET( at_uart, name ) \
    SEND_AT_CMD_OR_RET( at_uart, _r, name )

  /**
   * @brief Set AT command, returns in case of failure
   * @param name AT command name
   * @param params A string which contains command parameters
   */
  #define SEND_AT_CMD_S_OR_RET( at_uart, name, params ) \
    SEND_AT_CMD_OR_RET( at_uart, _s, name, params )
  
  typedef enum at_uart_err {
    AT_UART_OK          = 0,
    AT_UART_TIMEOUT     = -1,
    AT_UART_ERR         = -2,
    AT_UART_UNK         = -3,
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
  at_uart_err_t at_uart_setup( at_uart_t *at_uart, struct at_uart_config *at_uart_config );

  /**
   * @brief Tries to retrieve AT command result code from the given string
   * 
   * @param str_buf String buffer to check
   * @return at_uart_code_t 
   */
  at_uart_err_t at_uart_get_str_code( at_uart_t *at_uart, const char *str_buf );

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
  at_uart_err_t at_uart_write_cmd( at_uart_t *at_uart, char *cmd, size_t cmd_len );
  
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

  int8_t at_uart_set_dtr( at_uart_t *at_uart, uint8_t option );
  int8_t at_uart_set_flow_control( at_uart_t *at_uart, uint8_t option );
  int8_t at_uart_store_active_config( at_uart_t *at_uart, uint8_t profile );
  int8_t at_uart_set_reset_profile( at_uart_t *at_uart, uint8_t profile );
  int8_t at_uart_flush_to_eeprom( at_uart_t *at_uart );
  
#endif