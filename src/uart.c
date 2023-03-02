#include "uart.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

// minumum buffer size required to prase at least
// AT string codes
#define AT_MIN_BUFF_SIZE  32

// TODO: rename this module to at_uart.h
// TODO: functions should be prefixed witrh at_uart_*
// TODO: this will increase consistency

struct at_uart_buff {
  uint16_t rx_len;
  uint8_t rx[ RX_BUFF_SIZE ];
  
  uint16_t tx_len;
  uint16_t tx_idx;
  uint8_t tx[ TX_BUFF_SIZE ];
};

struct at_uart_queue {
  struct k_msgq rx_q;
  uint8_t rx_buff[ RX_MSGQ_LEN * sizeof(uint8_t) ];
};

struct at_uart {
  bool _echoed;
  struct at_uart_buff buff;
  struct at_uart_queue queue;
  struct at_uart_config config;
};

static struct at_uart g_uart;

// ------------- Private AT basic command methods ---------------
int8_t _at_uart_set_quiet( bool enable );
int8_t _at_uart_set_verbose( bool enable );
int8_t _at_uart_enable_echo( bool enable );
int8_t _at_uart_enable_flow_control( bool enable );
// --------- End of private AT basic command methods ------------

uint16_t at_uart_get_n_bytes( 
  uint8_t *bytes, uint16_t n_bytes, uint16_t timeout_ms 
) {
 
  uint8_t byte;
 
  while ( n_bytes > 0 
    && k_msgq_get( &g_uart.queue.rx_q, &byte, K_MSEC( timeout_ms ) ) == 0 ) {
    
    // ! Interrupts can disable RX interrupts when queue starts
    // ! filling up, which probably means instant packet loss
    // ! so instead disabling RX the queue will skip (silently) those bytes
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
    return ISBD_AT_OK;
  }

  uint8_t byte;
  uint16_t byte_i = 0;

  // This flag is used to avoid rechecking echo for segmented responses
  g_uart._echoed = true;

  while( k_msgq_get( &g_uart.queue.rx_q, &byte, K_MSEC( 100 ) ) == 0 ) {

    // TODO: Recheck this in a near future
    // ! If echo mode is enabled the ISU appends a \n char 
    // ! at the beginning of the AT command, but this only occurs
    // ! for a specific subset of commands so the problem is probably
    // ! not present in this library or maybe it's due to a misunderstanding
    if ( byte_i == 0 && (byte == '\r' || byte == '\n') ) {
      // printk( "TRAILING ... %d\n", byte );
      continue;
    }

    if ( g_uart.buff.tx[ byte_i ] != byte ) {
      printk("CHECK ERROR %d %d\n", byte, g_uart.buff.tx[ byte_i ] );
      // Echoed command do not matches previously transmitted command
      return ISBD_AT_ERR;
    }

    byte_i++;

    if ( byte_i == g_uart.buff.tx_len ) {

      // Echoed command matches
      return ISBD_AT_OK;
    }
  }

  // Echo timeout
  return ISBD_AT_TIMEOUT;
}

at_uart_code_t at_uart_pack_txt_resp(
  char *__str_resp, uint16_t str_resp_len, uint8_t lines, uint16_t timeout_ms 
) {
  
  uint8_t byte;
  uint8_t line_n = 1;

  uint16_t at_buff_i = 0;
  uint16_t str_resp_i = 0;

  at_uart_code_t at_code;
  
  // __str_resp buffer will be used to store the most relevant string response

  // this little __buff is used to parse AT responses
  char __at_buff[ AT_MIN_BUFF_SIZE ] = "";

  // TODO: timeout should be only for the first character
  while ( k_msgq_get( &g_uart.queue.rx_q, &byte, K_MSEC( timeout_ms ) ) == 0 ) {
    
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

    if ( at_buff_i > 0 && trail_char ) {
      
      // TODO: AT code should be checked only in the first and last line
      at_code = at_uart_get_str_code( __at_buff );

      if ( at_code != ISBD_AT_UNK ) {
        return at_code;
      }

      line_n++;

      if ( line_n > lines ) {
        return ISBD_AT_UNK;
      }

      if ( __str_resp && lines > 1 && line_n > 1 && line_n < lines ) {
        // add trailing char
      }

      at_buff_i = 0;
      __at_buff[ 0 ] = '\0';

    }

    if ( !trail_char ) {
      
      if ( __str_resp
        
        && str_resp_i < str_resp_len - 1  
        && ( lines == AT_1_LINE_RESP || line_n < lines) ) {
        __str_resp[ str_resp_i ] = byte;
        __str_resp[ str_resp_i + 1 ] = '\0';
      }

      // at buff is only used for AT command responses
      if ( at_buff_i < sizeof( __at_buff ) - 1 ) {
        __at_buff[ at_buff_i ] = byte;
        __at_buff[ at_buff_i + 1 ] = '\0';
      }

      at_buff_i++;
      str_resp_i++;

    }

  }

  return ISBD_AT_TIMEOUT;
}

at_uart_code_t at_uart_pack_txt_resp_code( int8_t *cmd_code, uint16_t timeout_ms ) {

  // ! The size of this buff needs at least the size to store
  // ! command response codes which usually are between 0 and 9
  char __cmd_code[ AT_MIN_BUFF_SIZE ] = "";

  at_uart_code_t at_code =
    at_uart_pack_txt_resp( __cmd_code, sizeof( __cmd_code ), AT_1_LINE_RESP, timeout_ms );
  
  if ( at_code == ISBD_AT_UNK ) {
    *cmd_code = atoi( __cmd_code );
    return ISBD_AT_OK;
  }

  return at_code;
}

inline at_uart_code_t at_uart_skip_txt_resp( 
  uint8_t lines, uint16_t timeout_ms 
) {
  return at_uart_pack_txt_resp( NULL, 0, lines, timeout_ms );
}

// TODO: implement a queue for TX buffer
uint16_t at_uart_write( uint8_t *__src_buf, uint16_t len ) {

  if ( len > TX_BUFF_SIZE ) {
    return 0;
  }

  g_uart.buff.tx_idx = 0; // reset transmission buffer index
  g_uart.buff.tx_len = len; // update transmission buffer length
  
  bytecpy( g_uart.buff.tx, __src_buf, len ); // copy at buffer to transmission

  uart_irq_tx_enable( g_uart.config.dev );

  return len;
}

at_uart_code_t at_uart_write_cmd( uint8_t *__src_buf, uint16_t len ) {
    
  g_uart._echoed = false;
  k_msgq_purge( &g_uart.queue.rx_q );

  at_uart_write( __src_buf, len );

  return at_uart_check_echo();
}

at_uart_code_t at_uart_get_str_code( const char *__buff ) {

  if ( g_uart.config.verbose ) {
    // ordered by occurrence frequency
    if ( strcmp( __buff, "OK" ) == 0 ) {
      return ISBD_AT_OK;
    } else if ( strcmp( __buff, "ERROR" ) == 0 ) {
      return ISBD_AT_ERR;
    } else if ( strcmp( __buff, "SBDRING" ) == 0 ) {
      return ISBD_AT_RING;
    }
  } else {
    if ( strcmp( __buff, "0" ) == 0 ) {
      return ISBD_AT_OK;
    } else if ( strcmp( __buff, "4" ) == 0 ) {
      return ISBD_AT_ERR;
    } else if ( strcmp( __buff, "126" ) == 0 ) {
      return ISBD_AT_RING;
    }
  }
  
  // READY is the same for both modes
  if ( strcmp( __buff, "READY" ) == 0 ) {
    return ISBD_AT_RDY;
  }

  return ISBD_AT_UNK;
}

/*
isbd_err_t at_uart_clear_rx() {
  // k_msgq_purge( &g_uart.queue.rx_q );
}
*/


void _uart_tx_isr( const struct device *dev, void *user_data ) {

  uint8_t *tx_buff = g_uart.buff.tx;
  uint16_t *tx_buff_len = &g_uart.buff.tx_len;
  uint16_t *tx_buff_idx = &g_uart.buff.tx_idx;
  
  if ( *tx_buff_idx < *tx_buff_len ) {
    (*tx_buff_idx) += uart_fifo_fill( 
      dev, &tx_buff[ *tx_buff_idx ], *tx_buff_len - *tx_buff_idx );
  }
  
  if ( *tx_buff_idx == *tx_buff_len ) {
  
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

}

at_uart_code_t at_uart_setup( struct at_uart_config *at_uart_config ) {

  g_uart.buff.rx_len = 0;
  g_uart.buff.tx_len = 0;

  // update whole configuration
  g_uart.config = *at_uart_config;



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

  return ISBD_AT_OK; 
}

// ------ Non propietary AT basic commands implementation ------

int8_t at_uart_set_flow_control( uint8_t option ) {
  SEND_AT_CMD_P( "&K", option );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

int8_t at_uart_set_dtr( uint8_t option ) {
  SEND_AT_CMD_P( "&D", option );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

int8_t _at_uart_set_quiet( bool enable ) {
  SEND_AT_CMD_P( "Q", enable );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

int8_t _at_uart_enable_echo( bool enable ) {
  g_uart.config.echo = enable;
  SEND_AT_CMD_P( "E", enable );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

int8_t _at_uart_set_verbose( bool enable ) {
  g_uart.config.verbose = enable;
  SEND_AT_CMD_P( "V", enable );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

// ---- End of propietary AT basic commands implementation -----
