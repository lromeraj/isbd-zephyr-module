#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/drivers/uart.h>

#include "at.h"
#include "isbd.h"

#define ISBD_UART_MSGS_BUF( name ) \
  char *name[ RX_MSGQ_LEN ] = { NULL };

struct isbd {
  struct isbd_config config;
};

static struct isbd g_isbd = {};

at_uart_code_t _isbd_pack_bin_resp( 
  uint8_t *__msg, uint16_t *msg_len, uint16_t *csum, uint16_t timeout_ms
);
int8_t _isbd_using_three_wire_connection( bool using );

isbd_err_t isbd_setup( struct isbd_config *isbd_config ) {
 
  g_isbd.config = *isbd_config;

  at_uart_code_t at_code = at_uart_setup( &isbd_config->at_uart );

  struct uart_config config;
  uart_config_get( isbd_config->at_uart.dev, &config );

  // ! Enable or disable flow control depending on uart configuration
  // ! this will avoid hangs during communication
  // ! Remember that the ISU transits between different states
  // ! depending on some circumstances, but for AT commands
  // ! flow control is implicitly disabled

  if ( config.flow_ctrl == UART_CFG_FLOW_CTRL_NONE ) {
    at_code = _isbd_using_three_wire_connection( true );
  } else {
    at_code = _isbd_using_three_wire_connection( false );
  }
  
  return at_code == ISBD_AT_OK ? ISBD_OK : ISBD_ERR;
}


int8_t isbd_store_active_config( uint8_t profile ) {
  SEND_AT_CMD_P( "&W", profile );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

int8_t isbd_set_reset_profile( uint8_t profile ) {
  SEND_AT_CMD_P( "&Y", profile );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

int8_t isbd_flush_to_eeprom() {
  SEND_AT_CMD_E( "*F" );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}


int8_t isbd_get_imei( char *__imei, uint16_t imei_len ) {
  SEND_AT_CMD_E( "+cgsn" );
  return at_uart_pack_txt_resp( __imei, imei_len, AT_2_LINE_RESP, 1000 );
}

int8_t isbd_get_revision( char *__revision, uint16_t revision_len ) {
  SEND_AT_CMD_E( "+cgmr" );
  return at_uart_pack_txt_resp( __revision, revision_len, 10, 100 );
}

int8_t isbd_get_rtc( char *__rtc, uint16_t rtc_len ) {
  SEND_AT_CMD_E( "+cclk" );
  return at_uart_pack_txt_resp( __rtc, rtc_len, AT_2_LINE_RESP, 100 );
}

int8_t isbd_init_session( isbd_session_t *session ) {

  char __buff[ 64 ];
  
  at_uart_code_t at_code;
  SEND_AT_CMD_E( "+sbdix" );

  at_code = at_uart_pack_txt_resp( 
    __buff, sizeof( __buff ), AT_2_LINE_RESP, 10000 );
  
  // TODO: implement optimized function instead of using sscanf
  sscanf( __buff, "+SBDIX:%hhu,%hu,%hhu,%hu,%hu,%hhu",
    &session->mo_sts,
    &session->mo_msn,
    &session->mt_sts,
    &session->mt_msn,
    &session->mt_len,
    &session->mt_queued );

  return at_code;
}

// TODO: consider using int8_t as parameter
int8_t isbd_clear_buffer( isbd_clear_buffer_t buffer ) {

  int8_t cmd_code;
  at_uart_code_t at_code;
  SEND_AT_CMD_P( "+sbdd", buffer );

  // retrieve command response code
  // this is not AT command interface result code
  at_code = at_uart_pack_txt_resp_code( &cmd_code, 1000 );

  if ( at_code == ISBD_AT_OK ) {
    at_code = at_uart_skip_txt_resp( AT_1_LINE_RESP, 1000 );
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
  SEND_AT_CMD_S( "+sbdwt", txt );
  return at_uart_skip_txt_resp( AT_2_LINE_RESP, 100 );
}

int8_t isbd_set_mo_bin( const uint8_t *__msg, uint16_t msg_len ) {

  uint8_t __data[ msg_len + 2 ];

  char __len[ 8 ];
  snprintf( __len, sizeof( __len ), "%d", msg_len );
  SEND_AT_CMD_S( "+sbdwb", __len );

  int8_t cmd_code;
  at_uart_code_t at_code;
  
  // After the initial AT+SBDWB command
  // the ISU should answer with a READY string
  // but if the length is not correct or some other validity check
  // fails, the resulting value will be a code corresponding to 
  // the command context and not to the AT command interface itself
  at_code = 
    at_uart_pack_txt_resp_code( &cmd_code, 100 );

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
    at_uart_write( __data, msg_len + 2 );

    // retrieve the command result code
    at_uart_pack_txt_resp_code( &cmd_code, 100 ); 
  }

  // always fetch last AT command
  at_code = at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );

  return at_code == ISBD_AT_OK ? cmd_code : at_code;
}

int8_t isbd_get_mt_bin( uint8_t *__msg, uint16_t *msg_len, uint16_t *csum ) {
  SEND_AT_CMD_E( "+sbdrb" );
  return _isbd_pack_bin_resp( __msg, msg_len, csum, 100 );
}

/*
int8_t isbd_set_mo_txt_l( char *__txt ) {
  
  SEND_AT_CMD_EXT( "sbdwt" );

  int8_t cmd_code;
  isbd_at_code_t at_code = 
    at_uart_pack_txt_resp_code( &cmd_code, 100 );
  
  printk("AT CODE: %d\n", at_code );

  if ( at_code == ISBD_AT_RDY ) {

    // TODO: add \r char

    uint16_t txt_len = strlen( __txt );
    _uart_write( (uint8_t*)__txt, txt_len );

    at_code = at_uart_pack_txt_resp( NULL, AT_2_LINE_RESP, 100 );
  }

  return cmd_code;
}
*/

int8_t isbd_mo_to_mt( char *__out, uint16_t out_len ) {
  SEND_AT_CMD_E( "+sbdtc" );
  return at_uart_pack_txt_resp( __out, out_len, AT_2_LINE_RESP, 1000 );
}

int8_t isbd_get_mt_txt( char *__mt_buff, uint16_t mt_buff_len ) {
  
  SEND_AT_CMD_E( "+sbdrt" );

  if ( g_isbd.config.at_uart.verbose ) {
    return at_uart_pack_txt_resp( 
      __mt_buff, mt_buff_len, AT_3_LINE_RESP, 100 );  
  } else {
    
    uint16_t len;
    at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
    at_uart_pack_txt_resp( __mt_buff, mt_buff_len, AT_1_LINE_RESP, 100 );
    
    len = strlen( __mt_buff ) - 1;
    
    __mt_buff[ len ] = 0;
    int8_t code = atoi( &__mt_buff[ len ] ); 
    
    return code;
  }

}

at_uart_code_t _isbd_pack_bin_resp( 
  uint8_t *__msg, uint16_t *msg_len, uint16_t *csum, uint16_t timeout_ms
) {

  uint8_t byte;
  
  // ! This was a bug caused by trailing chars
  // ! This has been fixed purging the queue just before
  // ! sending an AT command
  // // If verbose mode is enabled a trailing \r char 
  // // is used so we have to skip it
  // if ( g_isbd.config.verbose ) {
  //   _uart_get_n_bytes( &byte, 1, timeout_ms ); // \r
  // }

  at_uart_get_n_bytes( (uint8_t*)msg_len, 2, timeout_ms ); // message length
  *msg_len = ntohs( *msg_len );
  
  at_uart_get_n_bytes( __msg, *msg_len, timeout_ms );

  at_uart_get_n_bytes( (uint8_t*)csum, 2, timeout_ms );
  *csum = ntohs( *csum );

  return at_uart_skip_txt_resp( AT_1_LINE_RESP, timeout_ms );
}

int8_t _isbd_using_three_wire_connection( bool using ) {
  
  at_uart_code_t at_code;
  uint8_t en_param = using ? 0 : 3;

  at_code = at_uart_set_flow_control( en_param );

  if ( at_code == ISBD_AT_OK ) {
    at_code = at_uart_set_dtr( en_param );
  }

  return at_code;
}