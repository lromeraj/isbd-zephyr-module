#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/drivers/uart.h>

#include "at.h"
#include "isbd.h"

// minumum buffer size required to prase at least
// AT string codes
#define AT_MIN_BUFF_SIZE  32

// AT command response lines
#define AT_1_LINE_RESP    1
#define AT_2_LINE_RESP    2
#define AT_3_LINE_RESP    3

// AT command constraints
#define AT_SBDWT_MAX_LEN   120

#define RX_MSGQ_LEN     128

// TODO: think in reducing buffer sizes
#define TX_BUFF_SIZE    512 
#define RX_BUFF_SIZE    128

#define ISBD_UART_MSGS_BUF( name ) \
  char *name[ RX_MSGQ_LEN ] = { NULL };

struct uart_buff {
  uint16_t rx_len;
  uint8_t rx[ RX_BUFF_SIZE ];
  
  uint16_t tx_len;
  uint16_t tx_idx;
  uint8_t tx[ TX_BUFF_SIZE ];
};

struct uart_queue {
  struct k_msgq rx_q;
  uint8_t rx_buff[ RX_MSGQ_LEN * sizeof(uint8_t) ];
};

struct isbd {
  bool _echoed;
  struct uart_buff buff;
  struct uart_queue queue;   
  struct isbd_config config;
};

static struct isbd g_isbd = {};

/* ---------- Private methods ---------- */
isbd_err_t _uart_setup();
void _uart_write( uint8_t *__src_buf, uint16_t len );
uint16_t _uart_get_n_bytes( uint8_t *bytes, uint16_t n_bytes, uint16_t timeout_ms );
isbd_at_code_t _uart_check_echo();
isbd_at_code_t _uart_pack_bin_resp( uint8_t *__msg, uint16_t *msg_len, uint16_t *csum, uint16_t timeout_ms );
isbd_at_code_t _uart_skip_txt_resp( uint8_t lines, uint16_t timeout_ms );
isbd_at_code_t _uart_pack_txt_resp( char *__str_resp, uint16_t str_resp_len, uint8_t lines, uint16_t timeout_ms );
isbd_at_code_t _uart_pack_txt_resp_code( int8_t *cmd_code, uint16_t timeout_ms );
/* ------ End of private methods ------- */

isbd_err_t isbd_setup( struct isbd_config *config ) {
  g_isbd.config = *config;
  return _uart_setup();
}

void _uart_write( uint8_t *__src_buf, uint16_t len ) {

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

uint16_t _uart_get_n_bytes( 
  uint8_t *bytes, uint16_t n_bytes, uint16_t timeout_ms 
) {
 
  uint8_t byte;
 
  while ( n_bytes > 0 && k_msgq_get( &g_isbd.queue.rx_q, &byte, K_MSEC( timeout_ms ) ) == 0 ) {
    
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

inline uint16_t _uart_skip_n_bytes(
  uint16_t n_bytes, uint16_t timeout_ms
) {
  return _uart_get_n_bytes( NULL, n_bytes, timeout_ms );
}

isbd_at_code_t _uart_pack_bin_resp( 
  uint8_t *__msg, uint16_t *msg_len, uint16_t *csum, uint16_t timeout_ms
) {

  uint8_t byte;
  isbd_at_code_t code = _uart_check_echo();

  if ( code != ISBD_AT_OK ) {
    return code;
  }
  
  // ! This was a bug caused by trailing chars
  // ! This has been fixed purging the queue just before
  // ! sending an AT command
  // // If verbose mode is enabled a trailing \r char 
  // // is used so we have to skip it
  // if ( g_isbd.config.verbose ) {
  //   _uart_get_n_bytes( &byte, 1, timeout_ms ); // \r
  // }

  _uart_get_n_bytes( (uint8_t*)msg_len, 2, timeout_ms ); // message length
  *msg_len = ntohs( *msg_len );
  
  _uart_get_n_bytes( __msg, *msg_len, timeout_ms );

  _uart_get_n_bytes( (uint8_t*)csum, 2, timeout_ms );
  *csum = ntohs( *csum );

  return _uart_skip_txt_resp( AT_1_LINE_RESP, timeout_ms );
}

isbd_at_code_t _uart_check_echo() {

  if ( !g_isbd.config.echo || g_isbd._echoed ) {
    return ISBD_AT_OK;
  }

  uint8_t byte;
  uint16_t byte_i = 0;

  // This flag is used to avoid rechecking echo for segmented responses
  g_isbd._echoed = true;

  while( k_msgq_get( &g_isbd.queue.rx_q, &byte, K_MSEC( 100 ) ) == 0 ) {

    // TODO: Recheck this in a near future
    // ! If echo mode is enabled the ISU appends a \n char 
    // ! at the beginning of the AT command, but this only occurs
    // ! for a specific subset of commands so the problem is probably
    // ! not present in this library or maybe it's due to a misunderstanding
    if ( byte_i == 0 && (byte == '\r' || byte == '\n') ) {
      // printk( "TRAILING ... %d\n", byte );
      continue;
    }

    if ( g_isbd.buff.tx[ byte_i ] != byte ) {
      // printk("CHECK ERROR %d %d\n", byte, g_isbd.buff.tx[ byte_i ] );
      // Echoed command do not matches previously transmitted command
      return ISBD_AT_ERR;
    }

    byte_i++;
    if ( byte_i == g_isbd.buff.tx_len ) {

      // Echoed command matches
      return ISBD_AT_OK;
    }
  }

  // Echo timeout
  return ISBD_AT_TIMEOUT;
}

isbd_at_code_t _uart_pack_txt_resp( 
  char *__str_resp, uint16_t str_resp_len, uint8_t lines, uint16_t timeout_ms 
) {
  
  isbd_at_code_t at_code = _uart_check_echo();

  if ( at_code != ISBD_AT_OK ) {
    return at_code;
  }

  uint8_t byte;
  uint8_t line_n = 1;
  uint8_t buff_i = 0;

  // __str_resp buffer will be used to store the most relevant string response
  char *__buff;
  uint16_t buff_len = 0;

  // this little __buff is used to parse AT responses
  char __at_buff[ AT_MIN_BUFF_SIZE ] = "";
  
  if ( __str_resp && lines <= AT_2_LINE_RESP ) {
    __buff = __str_resp;
    buff_len = str_resp_len;
  } else {
    __buff = __at_buff;
    buff_len = sizeof( __at_buff );
  }

  // TODO: timeout should be only for the first character
  while ( k_msgq_get( &g_isbd.queue.rx_q, &byte, K_MSEC( timeout_ms ) ) == 0 ) {
    
    // ! If queue is full rx will be disabled,
    // ! so we have to reenable rx interrupts
    // uart_irq_rx_enable( g_isbd.config.dev );

    // printk( "LINE: %d / %d\n", line_n, lines );
  
    uint8_t trail_char = 0;


    if ( byte == '\r' || byte == '\n' ) {
      trail_char = byte;
      // printk( "CHAR: %d\n", byte );
    } else {
      // printk( "CHAR: %c %d/%d\n", byte, buff_i, buff_len );
    }

    if ( buff_i > 0 && trail_char ) {
      
      // TODO: AT code should be checked only in the first and penultimate line
      at_code = _at_get_msg_code( __buff );

      if ( at_code != ISBD_AT_UNK ) {
        return at_code;
      }

      line_n++;

      if ( line_n > lines ) {
        return ISBD_AT_UNK;
      }

      if ( __str_resp && (line_n == lines-1) ) {
        __buff = __str_resp;
        buff_len = str_resp_len;
      } else {
        __buff = __at_buff;
        buff_len = sizeof( __at_buff );
      } 

      buff_i = 0;
      __buff[ 0 ] = '\0';

    }

    if ( !trail_char ) {
      
      if ( buff_i < buff_len - 1 ) {
        __buff[ buff_i ] = byte;
        __buff[ buff_i + 1 ] = '\0';
      }

      buff_i++;
    }

  }

  return ISBD_AT_TIMEOUT;
}

inline isbd_at_code_t _uart_skip_txt_resp( 
  uint8_t lines, uint16_t timeout_ms 
) {
  return _uart_pack_txt_resp( NULL, 0, lines, timeout_ms );
}

static __AT_BUFF( __g_at_buf );

#define SEND_AT_CMD( fn, ... ) \
  g_isbd._echoed = false; \
  k_msgq_purge( &g_isbd.queue.rx_q ); \
  _uart_write( __g_at_buf, at_cmd##fn ( __g_at_buf, __VA_ARGS__ ) )

#define SEND_AT_CMD_P( name, param ) \
  SEND_AT_CMD( _p, name, param )

#define SEND_AT_CMD_EXT( name ) \
  SEND_AT_CMD( _ext, name )

#define SEND_AT_CMD_EXT_P( name, param ) \
  SEND_AT_CMD( _ext_p, name, param )

#define SEND_AT_CMD_EXT_SET( name, params ) \
  SEND_AT_CMD( _ext_s, name, params )

int8_t isbd_enable_flow_control( bool enable ) {

  isbd_at_code_t at_code;
  uint8_t en_param = enable ? 3 : 0;
  
  SEND_AT_CMD_P( "&K", en_param );
  at_code = _uart_skip_txt_resp( AT_1_LINE_RESP, 1000 );

  if ( at_code != ISBD_AT_OK ) {
    return at_code;
  } else {
    SEND_AT_CMD_P( "&D", en_param );
    at_code = _uart_skip_txt_resp( AT_1_LINE_RESP, 1000 );
  }

  return at_code;
}

int8_t isbd_enable_echo( bool enable ) {
  g_isbd.config.echo = enable;
  SEND_AT_CMD_P( "E", enable );
  return _uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

int8_t isbd_set_verbose( bool enable ) {
  g_isbd.config.verbose = enable;
  SEND_AT_CMD_P( "V", enable );
  return _uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

int8_t isbd_fetch_imei( char *__imei, uint16_t imei_len ) {
  SEND_AT_CMD_EXT( "cgsn" );
  return _uart_pack_txt_resp( __imei, imei_len, AT_2_LINE_RESP, 1000 );
}

int8_t isbd_fetch_revision( char *__revision, uint16_t revision_len ) {
  SEND_AT_CMD_EXT( "cgmr" );
  return _uart_pack_txt_resp( __revision, revision_len, AT_2_LINE_RESP, 100 );
}

int8_t isbd_fetch_rtc( char *__rtc, uint16_t rtc_len ) {
  SEND_AT_CMD_EXT( "cclk" );
  return _uart_pack_txt_resp( __rtc, rtc_len, AT_2_LINE_RESP, 100 );
}

int8_t isbd_init_session( isbd_session_t *session ) {

  char __buff[ 64 ];

  SEND_AT_CMD_EXT( "sbdix" );

  isbd_at_code_t at_code = 
    _uart_pack_txt_resp( __buff, sizeof( __buff ), AT_2_LINE_RESP, 10000 );
  
  // TODO: implement optimized function instead of using sscanf
  sscanf( __buff, "+SBDIX:%hhu,%hu,%hhu,%hu,%hu,%hhu",
    &session->mo_sts,
    &session->mo_msn,
    &session->mt_sts,
    &session->mt_msn,
    &session->mt_len,
    &session->mt_queued );

  return -1;
}

int8_t isbd_clear_buffer( isbd_clear_buffer_t buffer ) {

  int8_t cmd_code;
  SEND_AT_CMD_EXT_P( "SBDD", buffer );

  // retrieve command response code
  isbd_at_code_t at_code =
    _uart_pack_txt_resp_code( &cmd_code, 1000 );

  if ( at_code == ISBD_AT_OK ) {
    at_code = _uart_skip_txt_resp( AT_1_LINE_RESP, 1000 );
  }

  if ( at_code == ISBD_AT_OK ) {
    return cmd_code;
  }

  return at_code;
}

int8_t isbd_set_mo_txt( const char *txt ) {

  // ! This has been fixed, no need to append any extra trailing char
  // ! The length of text message is limited to 120 characters. 
  // ! This is due to the length limit on the AT command line interface. 
  // char __txt[ AT_SBDWT_MAX_LEN + 1 ];

  // ! If verbose mode is NOT enabled the ISU (Initial Signal Unit) returns 
  // ! the response code just after mobile terminated buffer when using +SBDRT
  // ! without any split char like \r or \n.
  // ! So we have to include manually a trailing \n
  // uint16_t len = snprintf( __txt, sizeof( __txt ), "%s", txt );

  /*
  if ( len > AT_SBDWT_MAX_LEN ) {
    __txt[ AT_SBDWT_MAX_LEN - 1 ] = '\n';
  }
  */

  SEND_AT_CMD_EXT_SET( "sbdwt", txt );
  return _uart_skip_txt_resp( AT_2_LINE_RESP, 100 );
}

isbd_at_code_t _uart_pack_txt_resp_code( int8_t *cmd_code, uint16_t timeout_ms ) {

  // ! The size of this buff needs at least the size to store
  // ! AT response messages (not user string responses)
  char __cmd_code[ AT_MIN_BUFF_SIZE ];

  isbd_at_code_t at_code = 
    _uart_pack_txt_resp( __cmd_code, sizeof( __cmd_code ), AT_1_LINE_RESP, timeout_ms );
  
  if ( at_code == ISBD_AT_UNK ) {
    *cmd_code = atoi( __cmd_code );
    return ISBD_AT_OK;
  }

  return at_code;
}

int8_t isbd_set_mo_bin( const uint8_t *__msg, uint16_t msg_len ) {

  uint8_t __data[ msg_len + 2 ];

  char __len[ 8 ];
  snprintf( __len, sizeof( __len ), "%d", msg_len );
  SEND_AT_CMD_EXT_SET( "sbdwb", __len );

  int8_t cmd_code;

  // After the initial AT+SBDWB command
  // the ISU should answer with a READY string
  // but if the length is not correct or some other validity check
  // fails, the resulting value will be a code corresponding to 
  // the command context and not to the AT interface itself
  isbd_at_code_t at_code = 
    _uart_pack_txt_resp_code( &cmd_code, 100 );

  if ( at_code == ISBD_AT_RDY ) {

    // compute message checksum
    uint32_t sum = 0;
    for ( int i=0; i < msg_len; i++ ) {
      sum += __data[ i ] = __msg[ i ];
    }

    // TODO: __data buffer is not mandatory and should be
    // TODO: removed in future modifications
    uint16_t *csum = (uint16_t*)&__data[ msg_len ];
    *csum = htons( sum & 0xFFFF ); 

    // finally write binary data to the ISU
    // MSG (N bytes) + CHECKSUM (2 bytes)
    _uart_write( __data, msg_len + 2 );

    // retrieve the command result code
    _uart_pack_txt_resp_code( &cmd_code, 100 );

  }

  // always fetch last AT command
  at_code = _uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
  
  return at_code == ISBD_AT_OK ? cmd_code : at_code;
}

int8_t isbd_get_mt_bin( uint8_t *__msg, uint16_t *msg_len, uint16_t *csum ) {
  SEND_AT_CMD_EXT( "sbdrb" );
  return _uart_pack_bin_resp( __msg, msg_len, csum, 100 );
}

// int8_t isbd_set_mo_txt_l( char *__txt ) {
  
//   SEND_AT_CMD_EXT( "sbdwt" );

//   int8_t cmd_code;
//   isbd_at_code_t at_code = 
//     _uart_pack_txt_resp_code( &cmd_code, 100 );
  
//   printk("AT CODE: %d\n", at_code );

//   if ( at_code == ISBD_AT_RDY ) {

//     // TODO: add \r char

//     uint16_t txt_len = strlen( __txt );
//     _uart_write( (uint8_t*)__txt, txt_len );

//     at_code = _uart_pack_txt_resp( NULL, AT_2_LINE_RESP, 100 );
//   }

//   return cmd_code;
// }

int8_t isbd_mo_to_mt( char *__out, uint16_t out_len ) {
  SEND_AT_CMD_EXT( "sbdtc" );
  return _uart_pack_txt_resp( __out, out_len, AT_2_LINE_RESP, 1000 );
}

int8_t isbd_get_mt_txt( char *__mt_buff, uint16_t mt_buff_len ) {

  SEND_AT_CMD_EXT( "sbdrt" );

  if ( g_isbd.config.verbose ) {
    return _uart_pack_txt_resp( 
      __mt_buff, mt_buff_len, AT_3_LINE_RESP, 100 );  
  } else {
    
    uint16_t len;
    _uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
    _uart_pack_txt_resp( __mt_buff, mt_buff_len, AT_1_LINE_RESP, 100 );
    
    len = strlen( __mt_buff ) - 1;
    
    __mt_buff[ len ] = 0;
    int8_t code = atoi( &__mt_buff[ len ] ); 
    
    return code;
  }

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
  
  /*
  // if queue is full we can not fill more bytes  
  if ( k_msgq_num_used_get( &g_isbd.queue.rx_q ) == RX_MSGQ_LEN ) {
    uart_irq_rx_disable( g_isbd.config.dev );
    return;
  }
  */

  uint8_t byte;  

  if ( uart_fifo_read( dev, &byte, 1 ) == 1 ) {
    k_msgq_put( &g_isbd.queue.rx_q, &byte, K_NO_WAIT );
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
    sizeof(uint8_t),
    RX_MSGQ_LEN );

  uart_irq_callback_user_data_set( 
    g_isbd.config.dev, _uart_isr, NULL );

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
  uart_irq_tx_disable( g_isbd.config.dev );

  struct uart_config config;  
  uart_config_get( g_isbd.config.dev, &config );

  // ! The response code of this commands
  // ! are not checked due to the possibility of 
  // ! an initial conflicting configuration
  // ! If the result code is an error does not mean
  // ! that the change has not been applied (only for this specific cases)
  // ! The last flow control command applied will finally check if the configuration was
  // ! correctly applied to the ISU

  isbd_enable_echo( g_isbd.config.echo );
  isbd_set_verbose( g_isbd.config.verbose );

  isbd_at_code_t at_code;

  // ! Enable or disable flow control depending on uart configuration
  // ! this will avoid hangs during communication
  // ! Remember that the ISU transits between different states
  // ! depending on some circumstances, but for AT commands
  // ! flow control is implicitly disabled
  if ( config.flow_ctrl == UART_CFG_FLOW_CTRL_NONE ) {
    at_code = isbd_enable_flow_control( false );
  } else {
    at_code = isbd_enable_flow_control( true );
  }

  return at_code == ISBD_AT_OK ? ISBD_OK : ISBD_ERR;
}