#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "at.h"
#include "isbd.h"

// AT command response lines
#define AT_1_LINE_RESP    1
#define AT_2_LINE_RESP    2
#define AT_3_LINE_RESP    3

// AT command constraints
#define AT_SBDWT_MAX_LEN   120

#define RX_MSG_SIZE     2
#define RX_MSGQ_LEN     8

// TODO: think in reducing buffer sizes
#define TX_BUFF_SIZE    512 
#define RX_BUFF_SIZE    RX_MSG_SIZE

#define ISBD_UART_MSGS_BUF( name ) \
  char *name[ RX_MSGQ_LEN ] = { NULL };

struct uart_buff {
  uint16_t rx_len;
  uint8_t rx[ RX_BUFF_SIZE ];
  
  uint16_t tx_len;
  uint16_t tx_idx;
  uint8_t tx[ TX_BUFF_SIZE ];
};

struct uart_queue_msg {
  uint8_t len;
  uint8_t data[ RX_MSG_SIZE ];
};

struct uart_queue {
  struct k_msgq rx_q;
  uint8_t rx_buff[ RX_MSGQ_LEN * sizeof(struct uart_queue_msg) ];
};

struct isbd {
  struct uart_buff buff;
  struct uart_queue queue;   
  struct isbd_config config;
};

static struct isbd g_isbd = {};

/* ---------- Private methods ---------- */
isbd_err_t _uart_setup();
/* ------ End of private methods ------- */

isbd_err_t isbd_setup( struct isbd_config *config ) {
  g_isbd.config = *config;
  return _uart_setup();
}

void isbd_uart_write( uint8_t *__src_buf, uint16_t len ) {  

  #ifdef UART_TX_POLLING
    
    for (int i = 0; i < len; i++) {
      uart_poll_out( g_isbd.config.dev, __src_buf[ i ] );
    }

  #else

    g_isbd.buff.tx_idx = 0; // reset transmission buffer index
    g_isbd.buff.tx_len = len; // update transmission buffer length
    bytecpy( g_isbd.buff.tx, __src_buf, len ); // copy at buffer to transmission

    uart_irq_tx_enable( g_isbd.config.dev );

  #endif
}

isbd_at_code_t _at_get_msg_code( const char *__buff ) {

  if ( g_isbd.config.verbose ) {

    if ( strcmp( __buff, "OK" ) == 0 ) {
      return ISBD_AT_OK;
    } else if ( strcmp( __buff, "ERROR" ) == 0 ) {
      return ISBD_AT_ERR;
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

isbd_at_code_t _uart_pack_bin_resp( uint8_t *__msg, uint16_t *len, uint16_t *csum, uint16_t timeout_ms ) {
  
  uint16_t byte_i = 0;

  struct uart_queue_msg msg;
  uint8_t *__len = (uint8_t*)len;
  
  while ( k_msgq_get( &g_isbd.queue.rx_q, &msg, K_MSEC( timeout_ms ) ) == 0 ) {    
    
    for ( int i=0; i < msg.len; i++ ) {

      uint8_t byte = msg.data[ i ];

      printf( "byte = %d\n", byte );


      if ( byte_i == 0 ) {
        __len[ 0 ] = byte;
      } else if ( byte_i == 1 ) {
        __len[ 1 ] = byte;
      }

      
      byte_i++;
    } 

    // ! If queue is full rx will be disabled,
    // ! so we have to reenable rx interrupts
    uart_irq_rx_enable( g_isbd.config.dev );

  }

}

isbd_at_code_t _uart_pack_txt_resp( char *__line, uint8_t lines, uint16_t timeout_ms ) {
  
  uint8_t line_n = 1;
  uint8_t buff_i = 0;

  struct uart_queue_msg msg;

  // this little buff is used to parse AT codes
  // line buffer will be used to store the most relevant string response
  char __buff[ 128 ] = "";

  while ( k_msgq_get( &g_isbd.queue.rx_q, &msg, K_MSEC( timeout_ms ) ) == 0 ) {
    
    // ! If queue is full rx will be disabled,
    // ! so we have to reenable rx interrupts
    uart_irq_rx_enable( g_isbd.config.dev );

    for ( int i=0; i < msg.len; i++ ) {

      bool trail_char = false;
      uint8_t byte = msg.data[ i ];

      if ( byte == '\r' || byte == '\n' ) {
        trail_char = true;
        // printk("CHAR: %d\n", byte );
      } else {
        // printk("CHAR: %c\n", byte );
      }

      if ( buff_i > 0 && trail_char ) {
        
        // printk("__buff: %s\n", __buff );
        // printk("__line: %s\n", __line );

        isbd_at_code_t code = _at_get_msg_code( __buff );

        if ( code != ISBD_AT_UNK ) {
          // printk("CODE: %d\n", code );
          return code;
        }

        buff_i = 0;
        __buff[ 0 ] = '\0';

        // go to the next line
        line_n++;

        // printk( "LINE: %d %d\n", line_n, lines );

      }
      
      if ( !trail_char ) {
        
        if ( line_n == lines-1 ) { // get the most relevant line
          if ( __line ) {
            __line[ buff_i ] = byte;
            __line[ buff_i + 1 ] = '\0';
          }
        } else { // get smaller AT responses
          // printk("__buff @ %c\n", byte );
          __buff[ buff_i ] = byte;
          __buff[ buff_i + 1 ] = '\0';          
        }
        // buffer index should be increased although
        // the __line buffer is ignored
        buff_i++;

      }

    }
  
    
    
  }

  // timeout
  return ISBD_AT_UNK;
}

static __AT_BUFF( __g_at_buf );

#define SEND_AT_CMD( fn, ... ) \
  isbd_uart_write( __g_at_buf, at_cmd##fn ( __g_at_buf, __VA_ARGS__ ) )

#define SEND_AT_CMD_P( name, param ) \
  SEND_AT_CMD( _p, name, param )

#define SEND_AT_CMD_EXT( name ) \
  SEND_AT_CMD( _ext, name )

#define SEND_AT_CMD_EXT_SET( name, params ) \
  SEND_AT_CMD( _ext_s, name, params )

isbd_at_code_t isbd_enable_flow_control( bool enable ) {

  isbd_at_code_t code;
  uint8_t en_param = enable ? 3 : 0 ;
  
  SEND_AT_CMD_P( "&K", en_param );
  code = _uart_pack_txt_resp( NULL, AT_1_LINE_RESP, 1000 );

  SEND_AT_CMD_P( "&D", en_param );
  code = _uart_pack_txt_resp( NULL, AT_1_LINE_RESP, 1000 );

  return code;
}

isbd_at_code_t isbd_set_verbose( bool enable ) {
  SEND_AT_CMD_P( "V", enable );
  return _uart_pack_txt_resp( NULL, AT_1_LINE_RESP, 1000 );
}

isbd_at_code_t isbd_fetch_imei( char *__imei ) {
  SEND_AT_CMD_EXT( "cgsn" );
  return _uart_pack_txt_resp( __imei, AT_2_LINE_RESP, 1000 );
}

isbd_at_code_t isbd_fetch_revision( char *__revision ) {
  SEND_AT_CMD_EXT( "cgmr" );
  return _uart_pack_txt_resp( __revision, AT_2_LINE_RESP, 1000 );
}

isbd_at_code_t isbd_fetch_rtc( char *__rtc ) {
  SEND_AT_CMD_EXT( "cclk" );
  return _uart_pack_txt_resp( __rtc, AT_2_LINE_RESP, 1000 );
}


isbd_at_code_t isbd_set_mo_txt( const char *txt ) {

  // ! The length of text message is limited to 120 characters. 
  // ! This is due to the length limit on the AT command line interface. 
  char __txt[ AT_SBDWT_MAX_LEN + 1 ];

  // ! If verbose mode is NOT enabled the ISU (Initial Signal Unit) returns 
  // ! the response code just after mobile terminated buffer when using +SBDRT
  // ! without any split char like \r or \n.
  // ! So we have to include manually a trailing \n
  uint16_t len = snprintf( __txt, sizeof( __txt ), "%s\n", txt );

  if ( len > AT_SBDWT_MAX_LEN ) {
    __txt[ AT_SBDWT_MAX_LEN - 1 ] = '\n';
  }

  SEND_AT_CMD_EXT_SET( "sbdwt", __txt );

  return _uart_pack_txt_resp( NULL, AT_2_LINE_RESP, 1000 );
}

isbd_at_code_t isbd_set_mo_bin( uint8_t *__msg, uint16_t msg_len ) {

  uint32_t sum = 0;
  uint8_t __data[ msg_len + 2 ];

  for ( int i=0; i < msg_len; i++ ) {
    sum += __data[ i ] = __msg[ i ];
  }

  char __len[ 4 ];
  sprintf( __len, "%d", msg_len );
  SEND_AT_CMD_EXT_SET( "sbdwb", __len );

  isbd_at_code_t code =  _uart_pack_txt_resp( NULL, AT_1_LINE_RESP, 1000 );

  if ( code == ISBD_AT_RDY ) {

    // ready to send binary data
    uint8_t *_csum = (uint8_t*)&sum;

    __data[ msg_len ] = _csum[ 1 ];
    __data[ msg_len + 1 ] = _csum[ 0 ];
  } else {
    return code;
  }

  isbd_uart_write( __data, msg_len + 2 );

  return _uart_pack_txt_resp( NULL, AT_2_LINE_RESP, 1000 );
}

isbd_at_code_t isbd_get_mt_bin( uint8_t *__msg, uint16_t *msg_len ) {

  SEND_AT_CMD_EXT( "sbdrb" );
  
  uint16_t csum;
  return _uart_pack_bin_resp( __msg, msg_len, &csum, 1000 );
}

isbd_at_code_t isbd_set_mo_txt_l( const char *txt ) {
  
  SEND_AT_CMD_EXT( "sbdwt" );

  // wait for READY\r\n
  isbd_at_code_t code = 
    _uart_pack_txt_resp( NULL, AT_1_LINE_RESP, 1000 );

  if ( code == ISBD_AT_RDY ) {
    // TODO: add \r char
    isbd_uart_write( (uint8_t*)txt, strlen( txt ) );
  
    code = _uart_pack_txt_resp( NULL, AT_2_LINE_RESP, 1000 );
  }

  return code;
}

isbd_at_code_t isbd_mo_to_mt( char *__out ) {
  SEND_AT_CMD_EXT( "sbdtc" );
  return _uart_pack_txt_resp( __out, AT_2_LINE_RESP, 1000 );
}

isbd_at_code_t isbd_get_mt_txt( char *__mt_buff ) {
  SEND_AT_CMD_EXT( "sbdrt" );
  return _uart_pack_txt_resp( __mt_buff, AT_3_LINE_RESP, 1000 );
}

void _uart_tx_isr( const struct device *dev, void *user_data ) {

  uint8_t *tx_buff = g_isbd.buff.tx;
  uint16_t *tx_buff_len = &g_isbd.buff.tx_len;
  uint16_t *tx_buff_idx = &g_isbd.buff.tx_idx;
  
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

      *tx_buff_idx = 0;
      *tx_buff_len = 0;
      
      // ! When uart_irq_tx_disable() is called
      // ! the transmission is halted although 
      // ! the fifo was filled successfully
      // ! See: https://github.com/zephyrproject-rtos/zephyr/issues/10672
      uart_irq_tx_disable( dev );

    }

  }

}

void _uart_rx_isr( const struct device *dev, void *user_data ) {
  //printk("ISR!\n");
  // if queue is full we can not fill more bytes  
  if ( k_msgq_num_used_get( &g_isbd.queue.rx_q ) == RX_MSGQ_LEN ) {
    uart_irq_rx_disable( g_isbd.config.dev );
    return;
  }

  uint8_t byte; // TODO: this should be named byte
  
  uint8_t *rx_buff = g_isbd.buff.rx;
  uint16_t *rx_buff_len = &g_isbd.buff.rx_len;
  
  *rx_buff_len = uart_fifo_read( dev, rx_buff, RX_BUFF_SIZE );

  if ( *rx_buff_len > 0 ) {
    struct uart_queue_msg msg;
    msg.len = *rx_buff_len;
    bytecpy( msg.data, rx_buff, *rx_buff_len );
    
    k_msgq_put( &g_isbd.queue.rx_q, &msg, K_NO_WAIT );
  }

  /*
  // TODO: collect every possible char
  // TODO: if loop breaks send the message to the corresponding queue
  while ( uart_fifo_read( dev, &byte, 1 ) == 1 ) {

    // TODO: for binary data we cannot use this characters as reference
    if ( *rx_buff_len == RX_BUFF_SIZE ) {
      
      if ( *rx_buff_len > 0 ) {
        struct uart_queue_msg msg;
        msg.len = *rx_buff_len;
        bytecpy( msg.data, rx_buff, *rx_buff_len );
        k_msgq_put( &g_isbd.queue.rx_q, &msg, K_NO_WAIT );
      }

      *rx_buff_len = 0;

    }

    rx_buff[ (*rx_buff_len)++ ] = byte;

  }

  if ( *rx_buff_len > 0 ) {
    struct uart_queue_msg msg;
    msg.len = *rx_buff_len;
    bytecpy( msg.data, rx_buff, *rx_buff_len );
    k_msgq_put( &g_isbd.queue.rx_q, &msg, K_NO_WAIT );
  }

  *rx_buff_len = 0;
  */

}

void _uart_isr( const struct device *dev, void *user_data ) {

	if ( !uart_irq_update( dev ) ) {
		return;
	}

  if ( uart_irq_rx_ready( dev ) ) {
    _uart_rx_isr( dev, user_data );
  }

  #ifndef UART_TX_POLLLING
    if ( uart_irq_tx_ready( dev ) ) {
      _uart_tx_isr( dev, user_data );
    }
  #endif

}

isbd_err_t _uart_setup() {

  g_isbd.buff.rx_len = 0;
  g_isbd.buff.tx_len = 0;

  // initialize message queue
  k_msgq_init( 
    &g_isbd.queue.rx_q,
    g_isbd.queue.rx_buff,
    sizeof(struct uart_queue_msg), 
    RX_MSGQ_LEN );

  int ret = uart_irq_callback_user_data_set( 
      g_isbd.config.dev, _uart_isr, NULL );

  uart_irq_rx_disable( g_isbd.config.dev );
  uart_irq_tx_disable( g_isbd.config.dev );

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

	uart_irq_rx_enable( g_isbd.config.dev );

  struct uart_config config;  
  uart_config_get( g_isbd.config.dev, &config );

  // TODO: test AT connection

  if ( g_isbd.config.verbose ) {
    isbd_set_verbose( true );
  } else {
    isbd_set_verbose( false );
  }

  // ! Enable or disable flow control depending on uart configuration
  // ! this will avoid hangs during device control
  if ( config.flow_ctrl == UART_CFG_FLOW_CTRL_NONE ) {
    isbd_enable_flow_control( false );
  } else {
    isbd_enable_flow_control( true );
  }

  return ISBD_AT_OK;
}