#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "stru.h"

#include "at.h"
#include "at_uart.h"

// For reference: https://www.etsi.org/deliver/etsi_ts/127000_127099/127007/10.03.00_60/ts_127007v100300p.pdf

// Minumum buffer size required to parse 
// at least AT string codes
#define AT_MIN_BUFF_SIZE          32

// Default AT short response timeout
#define AT_SHORT_TIMEOUT          1000 // ms

// ------------- Private AT basic commands ---------------

/**
 * @brief Enables or disables AT quiet mode. If enabled there will not be
 * AT command responses. 
 * 
 * @note In most scenarios quiet mode should be disabled for parsing AT command
 * responses. This has been implemented for internal use only.
 * 
 * @param enable Enable or disable quiet mode
 * @return int8_t 
 */
static at_uart_err_t _at_uart_set_quiet( at_uart_t *at_uart, bool enable );

/**
 * @brief Enables or disables AT command echo. If echo is enabled, 
 * commands will be echoed back to the master device.
 * 
 * @param enable Enable or disable AT command echo
 * @return int8_t 
 */
static at_uart_err_t _at_uart_enable_echo( at_uart_t *at_uart, bool enable );

/**
 * @brief Enables or disables verbose mode. When enabled the
 * AT command responses will be a word like string, otherwise
 * an ASCII unsigned integer will be received.
 * 
 * @param enable 
 * @return int8_t 
 */
static at_uart_err_t _at_uart_set_verbose( at_uart_t *at_uart, bool enable );

/**
 * @brief Echo command characters.
 * 
 * @note If enabled, this is used as an extra check to know 
 * if the device receives correctly AT commands. 
 * This is usually not necessary except in cases
 * where connection is too noisy, and despite of this the AT command will be corrupted
 * and the device will respond with an ERROR anyway, so it's recommended to disable
 * echo in order to reduce RX serial traffic.
 * 
 * @param enable 
 * @return int8_t 
 */
static at_uart_err_t _at_uart_enable_echo( at_uart_t *at_uart, bool enable );

static at_uart_err_t _at_uart_three_wire_connection( at_uart_t *at_uart, bool using );

// --------- End of private AT basic commands ------------


at_uart_err_t at_uart_check_echo( at_uart_t *at_uart ) {

  if ( !at_uart->config.echo || at_uart->_echoed ) {
    return AT_UART_OK;
  }

  uint8_t byte;
  // uint16_t byte_i = 0;

  // This flag is used to avoid rechecking echo for segmented responses
  at_uart->_echoed = true;

  at_uart_err_t at_code = AT_UART_OK;

  while( zuart_read( &at_uart->zuart, &byte, 1, AT_SHORT_TIMEOUT ) == 1 ) {

    // if ( byte_i == 0 && byte == '\n' ) continue;

    // ! When a mismatch is detected we have to drop all remaining chars
    // ! In case of electrical noise, the trailing char may be different
    // ! and not detected, in that case a timeout error will be returned
    // ! In case of extreme electrical noise (long/unconnected wires) 
    // ! this loop could get stuck until \r char is randomly found
    // TODO: Illegal private access, create a function inside zuart module
    // TODO: to retrieve specific transmission byte

    // ! If we want to check echo like this we need an additional buffer
    // ! to store the transmitted command, otherwise we can skip the command completely
    // if ( at_uart->zuart.buf.tx[ byte_i ] != byte ) {
    //   // Echoed command do not matches previously transmitted command
    //   at_code = AT_UART_ERR;
    // }

    // byte_i++;

    if ( byte == '\r' ) {
      return at_code;
    }

  }

  return AT_UART_TIMEOUT;
}

at_uart_err_t at_uart_pack_txt_resp(
  at_uart_t *at_uart, char *buf, size_t buf_size, uint8_t lines, uint16_t timeout_ms 
) {
  
  uint8_t line_n = 1;
  unsigned char byte;

  uint16_t buf_i = 0;
  uint16_t at_buf_i = 0;

  at_uart_err_t at_code;

  // this little buffer is used to parse AT status codes like ERROR, OK, ...
  char at_buf[ AT_MIN_BUFF_SIZE ] = "";

  while ( zuart_read( &at_uart->zuart, &byte, 1, timeout_ms ) == 1 ) {

    unsigned char trail_char = 0;

    if ( byte == '\r' || byte == '\n' ) {
      trail_char = byte;
    }

    if ( at_buf_i > 0 && trail_char == at_uart->eol ) {
      
      // TODO: AT code should be checked only in the first and last line
      at_code = at_uart_get_str_code( at_uart, at_buf );

      if ( at_code != AT_UART_UNK ) {
        return at_code;
      }

      line_n++;

      if ( line_n > lines ) {
        return AT_UART_UNK;
      }

      at_buf_i = 0;
      at_buf[ 0 ] = '\0';

    }

    if ( !trail_char ) {
      
      if ( buf && ( lines == AT_1_LINE_RESP || line_n < lines) ) {

          if ( buf_i >= buf_size - 1 ) {

            // ! We are leaving all pending chars in the ring buffer
            // ! so is very importante to clear reception buffer before
            // ! sending a new command 
            return AT_UART_OVERFLOW;

          } else {

            // TODO: We should append trailing chars to source buf 
            // TODO: if response string has multiple lines
            buf[ buf_i ] = byte;

            // TODO: Put this char only when buffer is terminated
            buf[ buf_i + 1 ] = '\0';
          }

      }

      // at buff is only used for AT command responses
      if ( at_buf_i < sizeof( at_buf ) - 1 ) {
        at_buf[ at_buf_i ] = byte;
        at_buf[ at_buf_i + 1 ] = '\0';
      }

      buf_i++;
      at_buf_i++;

    }

  }

  return AT_UART_TIMEOUT;
}

at_uart_err_t at_uart_get_resp_code( 
  at_uart_t *at_uart, 
  char *str_buf, uint16_t str_buf_len, 
  int8_t *cmd_code, uint16_t timeout_ms 
) {

  at_uart_err_t at_code = at_uart_pack_txt_resp( 
    at_uart, str_buf, str_buf_len, AT_1_LINE_RESP, timeout_ms );
  
  unsigned char first_char = str_buf[ 0 ];

  // ! We should consider conflicts when verbose mode is disabled,
  // ! in which case returned AT interface codes are also numbers 
  if ( at_code == AT_UART_UNK && isdigit( first_char ) ) {
    *cmd_code = atoi( str_buf );
    return AT_UART_OK;
  } else if ( at_code == AT_UART_OK ) {
    *cmd_code = 0;
  }

  return at_code;
}

inline at_uart_err_t at_uart_skip_txt_resp(
  at_uart_t *at_uart, uint8_t lines, uint16_t timeout_ms 
) {
  return at_uart_pack_txt_resp( at_uart, NULL, 0, lines, timeout_ms );
}

at_uart_err_t at_uart_write( 
  at_uart_t *at_uart, const uint8_t *src_buf, uint16_t n_bytes, uint32_t timeout_ms
) {

  uint16_t bytes_written = zuart_write( 
    &at_uart->zuart, src_buf, n_bytes, timeout_ms );

  if ( bytes_written < n_bytes ) {
    
    if ( zuart_get_err( &at_uart->zuart ) == ZUART_ERR_TIMEOUT ) {
      return AT_UART_TIMEOUT;
    } else{
      return AT_UART_ERR;
    }

  } else {
    return AT_UART_OK;
  }

}

at_uart_err_t at_uart_read(
  at_uart_t *at_uart, uint8_t *out_buf, uint16_t n_bytes, uint32_t timeout_ms
) {

  uint16_t bytes_read = zuart_read( 
    &at_uart->zuart, out_buf, n_bytes, timeout_ms );

  if ( bytes_read < n_bytes ) {
    if ( zuart_get_err( &at_uart->zuart ) == ZUART_ERR_TIMEOUT ) {
      return AT_UART_TIMEOUT;
    } else{
      return AT_UART_ERR;
    }
  } else {
    return AT_UART_OK;
  }
}

at_uart_err_t at_uart_write_cmd( 
  at_uart_t *at_uart, char *cmd_buf, size_t cmd_len
) {

  at_uart->_echoed = false;
  
  // ! Clear reception buffer to clear data remnants
  zuart_drain( &at_uart->zuart );

  at_uart_err_t err = at_uart_write(
    at_uart, cmd_buf, cmd_len, AT_SHORT_TIMEOUT );
  
  if ( err == AT_UART_OK ) {
    return at_uart_check_echo( at_uart ); 
  } else {
    return err;
  }
  
}

at_uart_err_t at_uart_send_cmd( 
  at_uart_t *at_uart, 
  char *at_cmd_buf, uint16_t at_cmd_buf_len,
  const char *at_cmd_tmpl, ...
) {

  va_list args;
  va_start( args, at_cmd_tmpl );

  at_uart_err_t ret = 
    at_uart_send_vcmd( at_uart, at_cmd_buf, at_cmd_buf_len, at_cmd_tmpl, args ); 
  
  va_end( args );
  
  return ret;
}

at_uart_err_t at_uart_send_vcmd(
  at_uart_t *at_uart, 
  char *at_cmd_buf, uint16_t at_cmd_buf_len,
  const char *at_cmd_tmpl, va_list args
) {

  int ret = vsnprintf( 
    at_cmd_buf, at_cmd_buf_len, at_cmd_tmpl, args );

  if ( ret < 0 ) {
    return AT_UART_ERR;
  } else if ( ret >= at_cmd_buf_len ) {
    return AT_UART_OVERFLOW;
  }

  return at_uart_write_cmd( at_uart, at_cmd_buf, ret );
}

at_uart_err_t at_uart_get_str_code( at_uart_t *at_uart, const char *buf ) {

  if ( at_uart->config.verbose ) {
    // ordered by occurrence frequency
    if ( streq( buf, "OK" ) ) {
      return AT_UART_OK;
    } else if ( streq( buf, "ERROR" ) ) {
      return AT_UART_ERR;
    }
  } else {
    if ( streq( buf, "0" ) ) {
      return AT_UART_OK;
    } else if ( streq( buf, "4" ) ) {
      return AT_UART_ERR;
    }
  }

  return AT_UART_UNK;
}

at_uart_err_t at_uart_setup( 
  at_uart_t *at_uart, at_uart_config_t *at_uart_config 
) {

  // update whole configuration
  at_uart->config = *at_uart_config;

  if ( at_uart->config.verbose ) {
    at_uart->eol = '\n';
  } else {
    at_uart->eol = '\r';
  }

  // setup underlying uart
  zuart_setup( &at_uart->zuart, &at_uart_config->zuart );

  // ! Disable quiet mode in order to parse command results
  _at_uart_set_quiet( at_uart, false );

  // ! The response code of this commands
  // ! are not checked due to the possibility of 
  // ! an initial conflicting configuration
  // ! If the result code is an error does not mean
  // ! that the change has not been applied (only for this specific cases)
  // ! The last AT test command will finally check if the configuration was
  // ! correctly applied to the ISU
  _at_uart_enable_echo( at_uart, at_uart->config.echo );
  _at_uart_set_verbose( at_uart, at_uart->config.verbose );

  // ! Enable or disable flow control depending on uart configuration
  // ! this will avoid hangs during communication
  // ! Remember that the ISU transits between different states
  // ! depending under specific circumstances, but for AT commands
  // ! flow control is implicitly disabled
  at_uart_err_t at_code;
  struct uart_config config;
  
  uart_config_get( at_uart->zuart.dev, &config );

  if ( config.flow_ctrl == UART_CFG_FLOW_CTRL_NONE ) {
    at_code = _at_uart_three_wire_connection( at_uart, true );
  } else {
    at_code = _at_uart_three_wire_connection( at_uart, false );
  }

  return at_code; 
}

const char *at_uart_err_to_name( at_uart_err_t code ) {
  
  if ( code == AT_UART_OK ) {
    return "AT_UART_OK";
  } else if ( code == AT_UART_ERR ) {
    return "AT_UART_ERROR";
  } else if ( code == AT_UART_OVERFLOW ) {
    return "AT_UART_OVERFLOW";
  } else if ( code == AT_UART_TIMEOUT ) {
    return "AT_UART_TIMEOUT";
  }

  return "AT_UART_UNKNOWN";
}

// ------ Non proprietary AT basic commands implementation ------
at_uart_err_t at_uart_set_flow_control( at_uart_t *at_uart, uint8_t option ) {
  // AT_UART_SEND_OR_RET( at_uart, AT_CMD_TMPL_EXEC_INT, "&k", option );
  at_uart_err_t ret;
  AT_UART_SEND_TINY_CMD_OR_RET( 
    ret, at_uart, AT_CMD_TMPL_EXEC_INT, "&k", option );

  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

at_uart_err_t at_uart_set_dtr( at_uart_t *at_uart, uint8_t option ) {
  
  at_uart_err_t ret;
  AT_UART_SEND_TINY_CMD_OR_RET( 
    ret, at_uart, AT_CMD_TMPL_EXEC_INT, "&d", option );

  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

at_uart_err_t at_uart_store_active_config( at_uart_t *at_uart, uint8_t profile ) {
  
  at_uart_err_t ret; 
  AT_UART_SEND_TINY_CMD_OR_RET( 
    ret, at_uart, AT_CMD_TMPL_EXEC_INT, "&w", profile );

  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

at_uart_err_t at_uart_set_reset_profile( at_uart_t *at_uart, uint8_t profile ) {
  
  at_uart_err_t ret; 
  AT_UART_SEND_TINY_CMD_OR_RET( 
    ret, at_uart, AT_CMD_TMPL_EXEC_INT, "&y", profile );

  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

at_uart_err_t at_uart_flush_to_eeprom( at_uart_t *at_uart ) {

  at_uart_err_t ret;
  AT_UART_SEND_TINY_CMD_OR_RET( 
    ret, at_uart, AT_CMD_TMPL_EXEC, "*f" );

  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}
// ---- End of proprietary AT basic commands implementation -----


// ---------------- ! (Internal use only) ! --------------------

static at_uart_err_t _at_uart_set_quiet( at_uart_t *at_uart, bool enable ) {

  at_uart_err_t ret;
  AT_UART_SEND_TINY_CMD_OR_RET( 
    ret, at_uart, AT_CMD_TMPL_EXEC_INT, "q", enable );

  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

static at_uart_err_t _at_uart_enable_echo( at_uart_t *at_uart, bool enable ) {

  at_uart_err_t ret;
  at_uart->config.echo = enable;

  AT_UART_SEND_TINY_CMD_OR_RET( 
    ret, at_uart, AT_CMD_TMPL_EXEC_INT, "e", enable );
  
  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

static at_uart_err_t _at_uart_set_verbose( at_uart_t *at_uart, bool enable ) {
  
  at_uart_err_t ret;
  at_uart->config.verbose = enable;
  
  AT_UART_SEND_TINY_CMD_OR_RET( 
    ret, at_uart, AT_CMD_TMPL_EXEC_INT, "v", enable );
  
  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

static at_uart_err_t _at_uart_three_wire_connection( at_uart_t *at_uart, bool using ) {
  
  at_uart_err_t ret;
  uint8_t en_param = using ? 0 : 3;

  ret = at_uart_set_flow_control( at_uart, en_param );

  if ( ret == AT_UART_OK ) {
    ret = at_uart_set_dtr( at_uart, en_param );
  }

  return ret;
}

// -------------------------------------------------------------