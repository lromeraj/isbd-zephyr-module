#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "uart.h"
#include "utils.h"

// Minumum buffer size required to parse 
// at least AT string codes
#define AT_MIN_BUFF_SIZE          32

// Default AT short response timeout
#define AT_SHORT_TIMEOUT          1000 // ms


#define RX_MSGQ_LEN     256

// TODO: think in reducing buffer sizes
#define TX_BUFF_SIZE    512 
#define RX_BUFF_SIZE    128

struct at_uart_buff {
  size_t rx_len;
  uint8_t rx[ RX_BUFF_SIZE ];
  
  size_t tx_len;
  size_t tx_idx;
  uint8_t tx[ TX_BUFF_SIZE ];
};

struct at_uart_queue {
  struct k_msgq rx_q;
  uint8_t rx_buff[ RX_MSGQ_LEN * sizeof(uint8_t) ];
};

struct at_uart {
  bool _echoed;
  unsigned char eol; // end of line char
  struct at_uart_buff buff;
  struct at_uart_queue queue;
  struct at_uart_config config;
};

// TODO: Make this module "instanceable"
// TODO: https://glab.lromeraj.net/ucm/miot/tfm/iridium-sbd-library/-/issues/3
static struct at_uart g_uart;

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
static int8_t _at_uart_set_quiet( bool enable );

/**
 * @brief Enables or disables AT command echo. If echo is enabled, 
 * commands will be echoed back to the master device.
 * 
 * @param enable Enable or disable AT command echo
 * @return int8_t 
 */
static int8_t _at_uart_enable_echo( bool enable );

/**
 * @brief Enables or disables verbose mode. When enabled the
 * AT command responses will be a word like string, otherwise
 * an ASCII unsigned integer will be received.
 * 
 * @param enable 
 * @return int8_t 
 */
static int8_t _at_uart_set_verbose( bool enable );

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
int8_t _at_uart_enable_echo( bool enable );
// --------- End of private AT basic command methods ------------

uint16_t at_uart_get_n_bytes( 
  uint8_t *bytes, uint16_t n_bytes, uint16_t timeout_ms 
) {
 
  uint8_t byte;

  // this conversion takes a little due to arithmetic division
  // so we temporary store the value instead of recomputing for each loop
  k_timeout_t k_timeout = K_MSEC( timeout_ms );

  while ( n_bytes > 0
    && k_msgq_get( &g_uart.queue.rx_q, &byte, k_timeout ) == 0 ) {
    
    // ! Interrupts can disable RX when queue starts
    // ! filling up, which probably means instant packet loss
    // ! so instead of disabling RX, the queue will skip (silently) those bytes
    // uart_irq_rx_enable( g_isbd.config.dev );
    // printk( "bin: %c\n", byte );

    if ( bytes ) {
      *bytes++ = byte;
    }
    n_bytes--;
  }

  // TODO: Change return logic
  return n_bytes;
}

inline uint16_t at_uart_skip_n_bytes(
  uint16_t n_bytes, uint16_t timeout_ms
) {
  return at_uart_get_n_bytes( NULL, n_bytes, timeout_ms );
}

at_uart_code_t at_uart_check_echo() {

  if ( !g_uart.config.echo || g_uart._echoed ) {
    return AT_UART_OK;
  }

  uint8_t byte;
  uint16_t byte_i = 0;

  // This flag is used to avoid rechecking echo for segmented responses
  g_uart._echoed = true;

  at_uart_code_t at_code = AT_UART_OK;
  k_timeout_t k_timeout = K_MSEC( AT_SHORT_TIMEOUT );

  while( k_msgq_get( &g_uart.queue.rx_q, &byte, k_timeout ) == 0 ) {
    
    if ( byte_i == 0 && byte == '\n' ) continue;

    // ! When a mismatch is detected we have to drop all remaining chars
    // ! In case of electrical noise, the trailing char may be different
    // ! and not detected, in that case a timeout error will be returned
    // ! In case of extreme electrical noise (long/unconnected wires) 
    // ! this loop could get stuck until \r char is randomly found
    if ( g_uart.buff.tx[ byte_i ] != byte ) {
      // Echoed command do not matches previously transmitted command
      at_code = AT_UART_ERR;
    }

    byte_i++;

    if ( byte == '\r' ) {
      return at_code;
    }
  }

  return AT_UART_TIMEOUT;
}

at_uart_code_t at_uart_pack_txt_resp(
  char *buf, size_t buf_len, uint8_t lines, uint16_t timeout_ms 
) {
  
  uint8_t byte;
  uint8_t line_n = 1;

  uint16_t buf_i = 0;
  uint16_t at_buf_i = 0;

  at_uart_code_t at_code;
  k_timeout_t k_timeout = K_MSEC( timeout_ms );

  // this little buffer is used to parse AT status codes like ERROR, OK, READY ...
  char at_buf[ AT_MIN_BUFF_SIZE ] = "";

  // TODO: timeout should be only for the first character
  while ( k_msgq_get( &g_uart.queue.rx_q, &byte, k_timeout ) == 0 ) {
    
    // ! If queue is full rx will be disabled,
    // ! so we have to reenable rx interrupts
    // uart_irq_rx_enable( g_isbd.config.dev );

    // printk( "LINE: %d / %d\n", line_n, lines );
    uint8_t trail_char = 0;

    if ( byte == '\r' || byte == '\n' ) {
      trail_char = byte;
      // printk( "CHAR: %d\n", byte );
    } else {
      // printk( "CHAR: %c\n", byte );
    }

    if ( at_buf_i > 0 && trail_char == g_uart.eol ) {
      
      // TODO: AT code should be checked only in the first and last line
      at_code = at_uart_get_str_code( at_buf );

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

      at_buf_i++;
      buf_i++;

    }

  }

  return AT_UART_TIMEOUT;
}

at_uart_code_t at_uart_pack_txt_resp_code( int8_t *cmd_code, uint16_t timeout_ms ) {

  // ! The size of this buff needs at least the size to store
  // ! an AT command response code which usually are between 0 and 9
  char cmd_code_buf[ AT_MIN_BUFF_SIZE ] = "";

  at_uart_code_t at_code =
    at_uart_pack_txt_resp( cmd_code_buf, sizeof( cmd_code_buf ), AT_1_LINE_RESP, timeout_ms );
  
  // ! We should considere conflicts when verbose mode is disabled,
  // ! in which case returned AT interface codes are also numbers 
  if ( at_code == AT_UART_UNK ) {
    *cmd_code = atoi( cmd_code_buf );
    return AT_UART_OK;
  }

  return at_code;
}

inline at_uart_code_t at_uart_skip_txt_resp(
  uint8_t lines, uint16_t timeout_ms 
) {
  return at_uart_pack_txt_resp( NULL, 0, lines, timeout_ms );
}

// TODO: We could use a queue for transmission bytes
uint16_t at_uart_write( uint8_t *src_buf, size_t len ) {

  if ( len > TX_BUFF_SIZE ) {
    return 0;
  }

  g_uart.buff.tx_idx = 0; // reset transmission buffer index
  g_uart.buff.tx_len = len; // update transmission buffer length

  bytecpy( g_uart.buff.tx, src_buf, len ); // copy at buffer to transmission

  uart_irq_tx_enable( g_uart.config.dev );
  // printk( "%d\n", len );

  return len;
}

at_uart_code_t at_uart_write_cmd( char *src_buf, size_t len ) {

  g_uart._echoed = false;

  // if device tries to send async data before sending a command,
  // we have to skip those bytes (or save for later use)
  // this should not be necessary if flow control is enabled
  if ( k_msgq_num_used_get( &g_uart.queue.rx_q ) > 0 ) {

    // printk("Skipping line ...\n" );
    at_uart_skip_txt_resp( AT_1_LINE_RESP, AT_SHORT_TIMEOUT );

    // uint8_t byte;
    // k_timeout_t k_timeout = K_MSEC( AT_SHORT_TIMEOUT );
    /*
    while ( k_msgq_get( &g_uart.queue.rx_q, &byte, k_timeout ) == 0 ) {
      if ( byte == '\n' )
    }
    */
  }

  k_msgq_purge( &g_uart.queue.rx_q );

  // also fully purge queue
  at_uart_write( src_buf, len );

  return at_uart_check_echo();

}

at_uart_code_t at_uart_get_str_code( const char *buf ) {

  if ( g_uart.config.verbose ) {
    
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
  
  // READY is the same for both modes
  if ( streq( buf, "READY" ) ) {
    return AT_UART_RDY;
  }

  return AT_UART_UNK;
}

void _uart_tx_isr( const struct device *dev, void *user_data ) {

  uint8_t *tx_buff = g_uart.buff.tx;
  size_t *tx_buff_len = &g_uart.buff.tx_len;
  size_t *tx_buff_idx = &g_uart.buff.tx_idx;
  
  if ( *tx_buff_idx < *tx_buff_len ) {
    
    (*tx_buff_idx) += uart_fifo_fill(
      dev, &tx_buff[ *tx_buff_idx ], *tx_buff_len - *tx_buff_idx );

  } else {
  
    // ! This may cause _uart_tx_isr() to be unnecessary over-called
    // ! This has been fixed by allowing interrupt to exit multiple times
    // ! instead of blocking for one call
    // while ( !uart_irq_tx_complete( dev ) );

    if ( uart_irq_tx_complete( dev ) ) {

      // *tx_buff_idx = 0;
      // *tx_buff_len = 0;
      
      // ! When uart_irq_tx_disable() is called
      // ! the transmission is halted although 
      // ! the fifo was filled successfully
      // ! See: https://github.com/zephyrproject-rtos/zephyr/issues/10672
      uart_irq_tx_disable( dev );
    }

  }

}

void _uart_rx_isr( const struct device *dev, void *user_data ) {
  uint8_t byte;
  if ( uart_fifo_read( dev, &byte, 1 ) == 1 ) {
    k_msgq_put( &g_uart.queue.rx_q, &byte, K_NO_WAIT );
  }
}

void _uart_isr( const struct device *dev, void *user_data ) {

	if ( !uart_irq_update( dev ) ) {
		return;
	}

  if ( uart_irq_rx_ready( dev ) ) {
    _uart_rx_isr( dev, user_data );
  }

  if ( uart_irq_tx_ready( dev ) ) {
    _uart_tx_isr( dev, user_data );
  }

}

at_uart_code_t at_uart_setup( struct at_uart_config *at_uart_config ) {

  g_uart.buff.rx_len = 0;
  g_uart.buff.tx_len = 0;

  // update whole configuration
  g_uart.config = *at_uart_config;

  if ( g_uart.config.verbose ) {
    g_uart.eol = '\n';
  } else {
    g_uart.eol = '\r';
  }
  // initialize message queue
  k_msgq_init( 
    &g_uart.queue.rx_q,
    g_uart.queue.rx_buff,
    sizeof(uint8_t),
    RX_MSGQ_LEN );

  uart_irq_callback_user_data_set( 
    g_uart.config.dev, _uart_isr, NULL );

  /*
  if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
	}
  */

 	uart_irq_rx_enable( g_uart.config.dev );
  uart_irq_tx_disable( g_uart.config.dev );

  // ! Disable quiet mode in order
  // ! to parse command results
  _at_uart_set_quiet( false );

  // ! The response code of this commands
  // ! are not checked due to the possibility of 
  // ! an initial conflicting configuration
  // ! If the result code is an error does not mean
  // ! that the change has not been applied (only for this specific cases)
  // ! The last AT test command will finally check if the configuration was
  // ! correctly applied to the ISU
  _at_uart_enable_echo( g_uart.config.echo );
  _at_uart_set_verbose( g_uart.config.verbose );

  return AT_UART_OK; 
}

const char *at_uart_err_to_name( at_uart_code_t code ) {
  
  if ( code == AT_UART_OK ) {
    return "AT_UART_OK";
  } else if ( code == AT_UART_ERR ) {
    return "AT_UART_ERROR";
  } else if ( code == AT_UART_RDY ) {
    return "AT_UART_READY";
  } else if ( code == AT_UART_TIMEOUT ) {
    return "AT_UART_TIMEOUT";
  }

  return "AT_UART_UNKNOWN";
}

// ------ Non proprietary AT basic commands implementation ------

int8_t at_uart_set_flow_control( uint8_t option ) {
  SEND_AT_CMD_P_OR_RET( "&k", option );
  return at_uart_skip_txt_resp( 
    AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

int8_t at_uart_set_dtr( uint8_t option ) {
  SEND_AT_CMD_P_OR_RET( "&d", option );
  return at_uart_skip_txt_resp( 
    AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

int8_t at_uart_store_active_config( uint8_t profile ) {
  SEND_AT_CMD_P_OR_RET( "&w", profile );
  return at_uart_skip_txt_resp( 
    AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

int8_t at_uart_set_reset_profile( uint8_t profile ) {
  SEND_AT_CMD_P_OR_RET( "&y", profile );
  return at_uart_skip_txt_resp( 
    AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

int8_t at_uart_flush_to_eeprom() {
  SEND_AT_CMD_E_OR_RET( "*f" );
  return at_uart_skip_txt_resp( 
    AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

static int8_t _at_uart_set_quiet( bool enable ) {
  SEND_AT_CMD_P_OR_RET( "q", enable );
  return at_uart_skip_txt_resp( 
    AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

static int8_t _at_uart_enable_echo( bool enable ) {
  g_uart.config.echo = enable;
  SEND_AT_CMD_P_OR_RET( "e", enable );
  return at_uart_skip_txt_resp( 
    AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

static int8_t _at_uart_set_verbose( bool enable ) {
  g_uart.config.verbose = enable;
  SEND_AT_CMD_P_OR_RET( "v", enable );
  return at_uart_skip_txt_resp( 
    AT_1_LINE_RESP, AT_SHORT_TIMEOUT );
}

// ---- End of proprietary AT basic commands implementation -----