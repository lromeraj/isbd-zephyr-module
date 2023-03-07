#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/drivers/uart.h>

#include "at.h"
#include "isbd.h"

/**
 * @brief Short timeout for commands which usually take
 * less than 1 second, for example: AT+CGSN
 */
#define SHORT_TIMEOUT_RESPONSE      1000 // ms

/**
 * @brief Long timeout for commands which take
 * more than 1 second, for example: AT+SBDI, AT+SBDIX, AT+CSQ, ...
 */
#define LONG_TIMEOUT_RESPONSE       (60 * 1000) // ms

struct isbd {
  struct isbd_config config;
};

static struct isbd g_isbd = {  };

static at_uart_code_t _isbd_pack_bin_resp( 
  uint8_t *msg_buf, size_t *msg_buf_len, uint16_t *csum, uint16_t timeout_ms 
);

static int8_t _isbd_using_three_wire_connection( bool using );

isbd_err_t isbd_setup( struct isbd_config *isbd_config ) {
 
  g_isbd.config = *isbd_config;

  at_uart_code_t at_code = at_uart_setup( &g_isbd.config.at_uart );

  struct uart_config config;
  uart_config_get( isbd_config->at_uart.dev, &config );

  // ! Enable or disable flow control depending on uart configuration
  // ! this will avoid hangs during communication
  // ! Remember that the ISU transits between different states
  // ! depending under specific circumstances, but for AT commands
  // ! flow control is implicitly disabled

  // TODO: Think about moving this function 
  // TODO: to the at_uart module
  if ( config.flow_ctrl == UART_CFG_FLOW_CTRL_NONE ) {
    at_code = _isbd_using_three_wire_connection( true );
  } else {
    at_code = _isbd_using_three_wire_connection( false );
  }

  return at_code == AT_UART_OK ? ISBD_OK : ISBD_ERR;
}

int8_t isbd_get_imei( char *imei_buf, size_t imei_buf_len ) {
  SEND_AT_CMD_E_OR_RET( "+cgsn" );
  return at_uart_pack_txt_resp(
    imei_buf, imei_buf_len, AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
}

int8_t isbd_get_revision( char *rev_buf, size_t rev_buf_len ) {
  SEND_AT_CMD_E_OR_RET( "+cgmr" );
  return at_uart_pack_txt_resp( 
    // TODO: Measure exact lines from AT+CGMR
    rev_buf, rev_buf_len, 10, SHORT_TIMEOUT_RESPONSE );
}

int8_t isbd_get_rtc( char *rtc_buf, size_t rtc_buf_len ) {
  SEND_AT_CMD_E_OR_RET( "+cclk" );
  return at_uart_pack_txt_resp( 
    rtc_buf, rtc_buf_len, AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
}

int8_t isbd_init_session( isbd_session_ext_t *session ) {

  char buf[ 64 ];
  
  at_uart_code_t at_code;
  SEND_AT_CMD_E_OR_RET( "+sbdix" );

  at_code = at_uart_pack_txt_resp(
    buf, sizeof( buf ), AT_2_LINE_RESP, LONG_TIMEOUT_RESPONSE );
  
  // TODO: implement optimized function instead of using sscanf
  sscanf( buf, "+SBDIX:%hhu,%hu,%hhu,%hu,%hu,%hhu",
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
  SEND_AT_CMD_P_OR_RET( "+sbdd", buffer );

  // retrieve command response code
  // this is not AT command interface result code
  at_code = at_uart_pack_txt_resp_code( 
    &cmd_code, SHORT_TIMEOUT_RESPONSE );

  if ( at_code == AT_UART_OK ) {
    at_code = at_uart_skip_txt_resp( 
      AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
  }

  if ( at_code == AT_UART_OK ) {
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

  SEND_AT_CMD_S_OR_RET( "+sbdwt", txt );
  return at_uart_skip_txt_resp( 
    AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
}

int8_t isbd_set_mo( const uint8_t *msg, size_t msg_len ) {

  // TODO: __data buffer is not mandatory and should be
  // TODO: removed in future modifications
  uint8_t tx_buf_size = msg_len + 2;
  uint8_t tx_buf[ tx_buf_size ];

  unsigned char msg_len_buf[ 8 ];
  snprintf( msg_len_buf, sizeof( msg_len_buf ), "%d", msg_len );

  SEND_AT_CMD_S_OR_RET( "+sbdwb", msg_len_buf );

  int8_t cmd_code;
  at_uart_code_t at_code;
  
  // After the initial AT+SBDWB command
  // the ISU should answer with a READY string
  // but if the length is not correct or some other validity check
  // fails, the resulting value will be a code corresponding to 
  // the command context and not to the AT command interface itself
  // So if the returned code from this function is not AT_UART_OK, cmd_code
  // will NOT be updated
  at_code = 
    at_uart_pack_txt_resp_code( &cmd_code, SHORT_TIMEOUT_RESPONSE );

  if ( at_code == AT_UART_RDY ) {

    // compute message checksum
    uint32_t sum = 0;
    for ( int i=0; i < msg_len; i++ ) {
      sum += tx_buf[ i ] = msg[ i ];
    }

    uint16_t *csum = (uint16_t*)&tx_buf[ msg_len ];
    *csum = htons( sum & 0xFFFF );

    // finally write binary data to the ISU
    // MSG (N bytes) + CHECKSUM (2 bytes)
    at_uart_write( tx_buf, tx_buf_size );

    // retrieve the command result code
    at_uart_pack_txt_resp_code( 
      &cmd_code, SHORT_TIMEOUT_RESPONSE ); 
  }

  // always fetch last AT command result
  at_code = at_uart_skip_txt_resp(
    AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  return at_code == AT_UART_OK ? cmd_code : at_code;
}

int8_t isbd_get_mt( uint8_t *__msg, size_t *msg_len, uint16_t *csum ) {
  SEND_AT_CMD_E_OR_RET( "+sbdrb" );
  return _isbd_pack_bin_resp(
    __msg, msg_len, csum, SHORT_TIMEOUT_RESPONSE );
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

int8_t isbd_mo_to_mt( char *__out, size_t out_len ) {
  SEND_AT_CMD_E_OR_RET( "+sbdtc" );
  return at_uart_pack_txt_resp(
    __out, out_len, AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
}

int8_t isbd_get_mt_txt( char *__mt_buff, size_t mt_buff_len ) {
  
  SEND_AT_CMD_E_OR_RET( "+sbdrt" );
  
  if ( g_isbd.config.at_uart.verbose ) {  
    return at_uart_pack_txt_resp( 
      __mt_buff, mt_buff_len, AT_3_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
  } else {
    
    uint16_t len;
    at_uart_skip_txt_resp( 
      AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
    
    at_uart_pack_txt_resp( 
      __mt_buff, mt_buff_len, AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
    
    len = strlen( __mt_buff ) - 1;
    
    __mt_buff[ len ] = 0;
    int8_t code = atoi( &__mt_buff[ len ] ); 
    
    return code;
  }

}

at_uart_code_t _isbd_pack_bin_resp(
  // TODO: timeout could be implicitly specified
  uint8_t *msg_buf, size_t *msg_buf_len, uint16_t *csum, uint16_t timeout_ms
) {

  at_uart_get_n_bytes( (uint8_t*)msg_buf_len, 2, timeout_ms ); // message length
  *msg_buf_len = ntohs( *msg_buf_len );

  at_uart_get_n_bytes( msg_buf, *msg_buf_len, timeout_ms );

  at_uart_get_n_bytes( (uint8_t*)csum, 2, timeout_ms );
  *csum = ntohs( *csum );

  return at_uart_skip_txt_resp( AT_1_LINE_RESP, timeout_ms );
}

int8_t isbd_get_sig_q( uint8_t *signal_q ) {

  char buf[ 16 ];
  SEND_AT_CMD_E_OR_RET( "+csq" );
  
  at_uart_code_t at_code = at_uart_pack_txt_resp(
    buf, sizeof( buf ), AT_2_LINE_RESP, LONG_TIMEOUT_RESPONSE );

  if ( at_code == AT_UART_OK ) {
    sscanf( buf, "+CSQ:%hhd", signal_q ); 
  }

  return at_code;
}

int8_t isbd_set_evt_report( evt_report_t *evt_report ) {

  char buf[16];

  snprintf( buf, sizeof( buf ), "%hhd,%hhd,%hhd", 
    evt_report->mode, evt_report->signal, evt_report->service );

  SEND_AT_CMD_S_OR_RET( "+cier", buf );

  // ! This command has a peculiarity,
  // ! after command is successfully executed, it returns an OK response,
  // ! but just after that it transmits the first indicator event
  // ! so we skip those lines
  at_uart_code_t at_code = at_uart_skip_txt_resp( AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );  

  uint8_t lines_to_skip = 
    evt_report->mode * ( evt_report->service + evt_report->signal );

  // TODO: we could return the resulting values instead of ignoring them ...
  if ( lines_to_skip > 0 ) {
    at_uart_skip_txt_resp( lines_to_skip, SHORT_TIMEOUT_RESPONSE );
  }

  return at_code;
}

static int8_t _isbd_using_three_wire_connection( bool using ) {
  
  at_uart_code_t at_code;
  uint8_t en_param = using ? 0 : 3;

  at_code = at_uart_set_flow_control( en_param );

  if ( at_code == AT_UART_OK ) {
    at_code = at_uart_set_dtr( en_param );
  }

  return at_code;
}