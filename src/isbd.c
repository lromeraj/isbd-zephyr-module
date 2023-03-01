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

static __AT_BUFF( __g_at_buf );

#define SEND_AT_CMD( at_code_var, fn, ... ) \
  at_code_var = at_uart_write_cmd( __g_at_buf, at_cmd##fn ( __g_at_buf, __VA_ARGS__ ) ); \
  if ( at_code_var != ISBD_AT_OK ) { return at_code_var; }

// exec
#define SEND_AT_CMD_E( at_code_var, name ) \
  SEND_AT_CMD(  at_code_var, _e, name )

// equivalent to exec but using extra param
#define SEND_AT_CMD_P( at_code_var, name, param ) \
  SEND_AT_CMD(  at_code_var, _p, name, param )

// test
#define SEND_AT_CMD_T( at_code_var, name ) \
  SEND_AT_CMD(  at_code_var, _t, name )

// read
#define SEND_AT_CMD_R( at_code_var, name, param ) \
  SEND_AT_CMD(  at_code_var, _r, name, param )

// set
#define SEND_AT_CMD_S( at_code_var, name, params ) \
  SEND_AT_CMD(  at_code_var, _s, name, params )

struct isbd {
  struct isbd_config config;
};

static struct isbd g_isbd = {};


// ------------- Private Iridium SBD methods ---------------
int8_t _isbd_set_quiet( bool enable );
int8_t _isbd_set_verbose( bool enable );
int8_t _isbd_enable_flow_control( bool enable );
// ---------- End of private Iridium SBD methods -----------

at_uart_code_t _isbd_pack_bin_resp( 
  uint8_t *__msg, uint16_t *msg_len, uint16_t *csum, uint16_t timeout_ms
);

isbd_err_t isbd_setup( struct isbd_config *config ) {
  
  g_isbd.config = *config;

  isbd_err_t err = at_uart_setup( &config->at_uart );

  if ( err == ISBD_OK ) {
    
    struct uart_config config;  
    uart_config_get( g_isbd.config.at_uart.dev, &config );

    // TODO: some of this functions/calls could be moved to
    // TODO: at_uart module
    // ! Disable quiet mode in order
    // ! to parse command results
    _isbd_set_quiet( false );

    // ! The response code of this commands
    // ! are not checked due to the possibility of 
    // ! an initial conflicting configuration
    // ! If the result code is an error does not mean
    // ! that the change has not been applied (only for this specific cases)
    // ! The last flow control command applied will finally check if the configuration was
    // ! correctly applied to the ISU
    isbd_enable_echo( g_isbd.config.at_uart.echo );
    _isbd_set_verbose( g_isbd.config.at_uart.verbose );

    at_uart_code_t at_code;

    // ! Enable or disable flow control depending on uart configuration
    // ! this will avoid hangs during communication
    // ! Remember that the ISU transits between different states
    // ! depending on some circumstances, but for AT commands
    // ! flow control is implicitly disabled
    if ( config.flow_ctrl == UART_CFG_FLOW_CTRL_NONE ) {
      at_code = _isbd_enable_flow_control( false );
    } else {
      at_code = _isbd_enable_flow_control( true );
    }

    err = ISBD_AT_OK ? ISBD_OK : ISBD_ERR;
  }

  return err;
}

int8_t _isbd_enable_flow_control( bool enable ) {
  
  at_uart_code_t at_code;
  uint8_t en_param = enable ? 3 : 0;

  SEND_AT_CMD_P( at_code, "&K", en_param );

  at_code = at_uart_skip_txt_resp( AT_1_LINE_RESP, 1000 );

  if ( at_code == ISBD_AT_OK ) {
    SEND_AT_CMD_P( at_code, "&D", en_param );
    at_code = at_uart_skip_txt_resp( AT_1_LINE_RESP, 1000 );
  }

  return at_code;
}

int8_t _isbd_set_quiet( bool enable ) {
  at_uart_code_t at_code;
  SEND_AT_CMD_P( at_code, "Q", enable );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

int8_t isbd_enable_echo( bool enable ) {
  at_uart_code_t at_code;
  g_isbd.config.at_uart.echo = enable;
  SEND_AT_CMD_P( at_code, "E", enable );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

int8_t _isbd_set_verbose( bool enable ) {
  at_uart_code_t at_code;
  g_isbd.config.at_uart.verbose = enable;
  SEND_AT_CMD_P( at_code, "V", enable );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

int8_t isbd_set_dtr( uint8_t option ) {
  at_uart_code_t at_code;
  SEND_AT_CMD_P( at_code, "&D", option );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

int8_t isbd_store_active_config( uint8_t profile ) {
  at_uart_code_t at_code;
  SEND_AT_CMD_P( at_code, "&W", profile );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

int8_t isbd_set_reset_profile( uint8_t profile ) {
  at_uart_code_t at_code;
  SEND_AT_CMD_P( at_code, "&Y", profile );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}

int8_t isbd_flush_to_eeprom() {
  at_uart_code_t at_code;
  SEND_AT_CMD_E( at_code, "*F" );
  return at_uart_skip_txt_resp( AT_1_LINE_RESP, 100 );
}


int8_t isbd_get_imei( char *__imei, uint16_t imei_len ) {
  at_uart_code_t at_code;
  SEND_AT_CMD_E( at_code, "+cgsn" );
  return at_uart_pack_txt_resp( __imei, imei_len, AT_2_LINE_RESP, 1000 );
}

int8_t isbd_get_revision( char *__revision, uint16_t revision_len ) {
  at_uart_code_t at_code;
  SEND_AT_CMD_E( at_code, "+cgmr" );
  return at_uart_pack_txt_resp( __revision, revision_len, 10, 100 );
}

int8_t isbd_get_rtc( char *__rtc, uint16_t rtc_len ) {
  at_uart_code_t at_code;
  SEND_AT_CMD_E( at_code, "+cclk" );
  return at_uart_pack_txt_resp( __rtc, rtc_len, AT_2_LINE_RESP, 100 );
}

int8_t isbd_init_session( isbd_session_t *session ) {

  char __buff[ 64 ];
  
  at_uart_code_t at_code;
  SEND_AT_CMD_E( at_code, "+sbdix" );

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
  SEND_AT_CMD_P( at_code, "+sbdd", buffer );

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
  at_uart_code_t at_code;
  SEND_AT_CMD_S( at_code, "+sbdwt", txt );
  return at_uart_skip_txt_resp( AT_2_LINE_RESP, 100 );
}

int8_t isbd_set_mo_bin( const uint8_t *__msg, uint16_t msg_len ) {

  at_uart_code_t at_code;
  uint8_t __data[ msg_len + 2 ];

  char __len[ 8 ];
  snprintf( __len, sizeof( __len ), "%d", msg_len );
  SEND_AT_CMD_S( at_code, "+sbdwb", __len );

  int8_t cmd_code;

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
  at_uart_code_t at_code;
  SEND_AT_CMD_E( at_code, "+sbdrb" );
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
  at_uart_code_t at_code;
  SEND_AT_CMD_E( at_code, "+sbdtc" );
  return at_uart_pack_txt_resp( __out, out_len, AT_2_LINE_RESP, 1000 );
}

int8_t isbd_get_mt_txt( char *__mt_buff, uint16_t mt_buff_len ) {
  
  at_uart_code_t at_code;
  SEND_AT_CMD_E( at_code, "+sbdrt" );

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

