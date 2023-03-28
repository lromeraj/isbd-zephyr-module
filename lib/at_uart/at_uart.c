#include <ctype.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "stru.h"
#include "at_uart.h"

// For reference: https://www.etsi.org/deliver/etsi_ts/127000_127099/127007/10.03.00_60/ts_127007v100300p.pdf

// Minumum buffer size required to parse 
// at least AT string codes
#define AT_MIN_BUFF_SIZE          32

// Default AT short response timeout
#define AT_SHORT_TIMEOUT          1000 // ms

// ------------- Private AT basic command methods ---------------
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
static int8_t _at_uart_set_quiet( at_uart_t *at_uart, bool enable );

/**
 * @brief Enables or disables AT command echo. If echo is enabled, 
 * commands will be echoed back to the master device.
 * 
 * @param enable Enable or disable AT command echo
 * @return int8_t 
 */
static int8_t _at_uart_enable_echo( at_uart_t *at_uart, bool enable );

/**
 * @brief Enables or disables verbose mode. When enabled the
 * AT command responses will be a word like string, otherwise
 * an ASCII unsigned integer will be received.
 * 
 * @param enable 
 * @return int8_t 
 */
static int8_t _at_uart_set_verbose( at_uart_t *at_uart, bool enable );


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
static int8_t _at_uart_enable_echo( at_uart_t *at_uart, bool enable );


static int8_t _at_uart_three_wire_connection( at_uart_t *at_uart, bool using );

// --------- End of private AT basic command methods ------------


at_uart_err_t at_uart_check_echo( at_uart_t *at_uart ) {

  if ( !at_uart->config.echo || at_uart->_echoed ) {
    return AT_UART_OK;
  }

  uint8_t byte;
  uint16_t byte_i = 0;

  // This flag is used to avoid rechecking echo for segmented responses
  at_uart->_echoed = true;

  at_uart_err_t at_code = AT_UART_OK;

  while( zuart_read( &at_uart->zuart, &byte, 1, AT_SHORT_TIMEOUT ) == 1 ) {
    
    if ( byte_i == 0 && byte == '\n' ) continue;

    // ! When a mismatch is detected we have to drop all remaining chars
    // ! In case of electrical noise, the trailing char may be different
    // ! and not detected, in that case a timeout error will be returned
    // ! In case of extreme electrical noise (long/unconnected wires) 
    // ! this loop could get stuck until \r char is randomly found
    // TODO: Illegal private access, create a function inside zuart module
    // TODO: to retrieve specific transmission byte

    // ! If we want to check echo like this we have an additional buffer
    // ! to store the transmitted command, otherwise we can skip the command completely
    // if ( at_uart->zuart.buf.tx[ byte_i ] != byte ) {
    //   // Echoed command do not matches previously transmitted command
    //   at_code = AT_UART_ERR;
    // }

    byte_i++;

    if ( byte == '\r' ) {
      return at_code;
    }
  }

  return AT_UART_TIMEOUT;
}

at_uart_err_t at_uart_pack_txt_resp(
  at_uart_t *at_uart, char *buf, size_t buf_len, uint8_t lines, uint16_t timeout_ms 
) {
  
  uint8_t byte;
  uint8_t line_n = 1;

  uint16_t buf_i = 0;
  uint16_t at_buf_i = 0;

  at_uart_err_t at_code;

  // this little buffer is used to parse AT status codes like ERROR, OK, READY ...
  char at_buf[ AT_MIN_BUFF_SIZE ] = "";
  while ( zuart_read( &at_uart->zuart, &byte, 1, timeout_ms ) == 1 ) {

    // printk( "LINE: %d / %d\n", line_n, lines );
    uint8_t trail_char = 0;

    if ( byte == '\r' || byte == '\n' ) {
      trail_char = byte;
      // printk( "CHAR: %d\n", byte );
    } else {
      // printk( "CHAR: %c\n", byte );
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
      
      if ( buf
        && buf_i < buf_len - 1
        && ( lines == AT_1_LINE_RESP || line_n < lines) ) {
        
        // TODO: We should append trailing chars to source buf 
        // TODO: if response string has multiple lines

        buf[ buf_i ] = byte;

        // TODO: Put this char only when buffer is terminated
        buf[ buf_i + 1 ] = '\0';
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

at_uart_err_t at_uart_get_cmd_resp_code( 
  at_uart_t *at_uart, int8_t *cmd_code, uint16_t timeout_ms 
) {

  // ! The size of this buff needs at least the size to store
  // ! an AT command response code which usually are between 0 and 9
  char cmd_code_buf[ AT_MIN_BUFF_SIZE ] = "";

  at_uart_err_t at_code = at_uart_pack_txt_resp( 
    at_uart, cmd_code_buf, sizeof( cmd_code_buf ), AT_1_LINE_RESP, timeout_ms );
  
  // ! We should considere conflicts when verbose mode is disabled,
  // ! in which case returned AT interface codes are also numbers 
  if ( at_code == AT_UART_UNK ) {
    *cmd_code = atoi( cmd_code_buf );
    return AT_UART_OK;
  }

  return at_code;
}

int16_t at_uart_pack_resp_code(
  at_uart_t *at_uart, char *str_code, uint16_t str_code_len, uint16_t timeout_ms
) {

  at_uart_err_t at_code = at_uart_pack_txt_resp( 
    at_uart, str_code, str_code_len, AT_1_LINE_RESP, timeout_ms );
  
  uint8_t first_char = str_code[ 0 ];
  
  // ! We should considere conflicts when verbose mode is disabled,
  // ! in which case returned AT interface codes are also numbers 
  if ( at_code == AT_UART_UNK && isdigit( first_char ) ) {
    return atoi( str_code );
  }

  return at_code;
}

inline at_uart_err_t at_uart_skip_txt_resp(
  at_uart_t *at_uart, uint8_t lines, uint16_t timeout_ms 
) {
  return at_uart_pack_txt_resp( at_uart, NULL, 0, lines, timeout_ms );
}

at_uart_err_t at_uart_write_cmd( 
  at_uart_t *at_uart, char *cmd_buf, size_t cmd_len
) {

  at_uart->_echoed = false;
  
  // ! if device tries to send async data before sending a command,
  // ! we have to skip those bytes (or save for later use)
  // ! this should not be necessary if flow control is enabled
  // if ( k_msgq_num_used_get( &g_uart.queue.rx_q ) > 0 ) {
  //   at_uart_skip_txt_resp( AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
  // }

  // also fully purge queue
  zuart_drain( &at_uart->zuart );

  int32_t ret = zuart_write( 
    &at_uart->zuart, cmd_buf, cmd_len, AT_SHORT_TIMEOUT );

  if ( ret == ZUART_ERR_TIMEOUT ) {
    return AT_UART_TIMEOUT;
  }

  return at_uart_check_echo( at_uart );
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
  } else if ( code == AT_UART_TIMEOUT ) {
    return "AT_UART_TIMEOUT";
  }

  return "AT_UART_UNKNOWN";
}

// ------ Non proprietary AT basic commands implementation ------
int8_t at_uart_set_flow_control( at_uart_t *at_uart, uint8_t option ) {
  SEND_AT_CMD_P_OR_RET( at_uart, "&k", option );
  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

int8_t at_uart_set_dtr( at_uart_t *at_uart, uint8_t option ) {
  SEND_AT_CMD_P_OR_RET( at_uart, "&d", option );
  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

int8_t at_uart_store_active_config( at_uart_t *at_uart, uint8_t profile ) {
  SEND_AT_CMD_P_OR_RET( at_uart, "&w", profile );
  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

int8_t at_uart_set_reset_profile( at_uart_t *at_uart, uint8_t profile ) {
  SEND_AT_CMD_P_OR_RET( at_uart, "&y", profile );
  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

int8_t at_uart_flush_to_eeprom( at_uart_t *at_uart ) {
  SEND_AT_CMD_E_OR_RET( at_uart, "*f" );
  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}
// ---- End of proprietary AT basic commands implementation -----

// ---------------- ! (Internal use only) ! --------------------

static int8_t _at_uart_set_quiet( at_uart_t *at_uart, bool enable ) {
  SEND_AT_CMD_P_OR_RET( at_uart, "q", enable );
  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

static int8_t _at_uart_enable_echo( at_uart_t *at_uart, bool enable ) {
  at_uart->config.echo = enable;
  SEND_AT_CMD_P_OR_RET( at_uart, "e", enable );
  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

static int8_t _at_uart_set_verbose( at_uart_t *at_uart, bool enable ) {
  at_uart->config.verbose = enable;
  SEND_AT_CMD_P_OR_RET( at_uart, "v", enable );
  return at_uart_skip_txt_resp( 
    at_uart, AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

static int8_t _at_uart_three_wire_connection( at_uart_t *at_uart, bool using ) {
  
  at_uart_err_t at_code;
  uint8_t en_param = using ? 0 : 3;

  at_code = at_uart_set_flow_control( at_uart, en_param );

  if ( at_code == AT_UART_OK ) {
    at_code = at_uart_set_dtr( at_uart, en_param );
  }

  return at_code;
}

// -------------------------------------------------------------