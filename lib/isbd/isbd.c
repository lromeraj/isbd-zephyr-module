#include <string.h>
#include <stdio.h>
#include <stdlib.h>
// #include <zephyr/kernel.h>
// #include <zephyr/device.h>
#include <zephyr/net/net_ip.h>
// #include <zephyr/drivers/uart.h>

#include "stru.h"
#include "at_uart.h"

#include "inc/isbd.h"

#define AT_READY_STR         "READY"

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

#define ISBD_SEND_TINY_CMD_OR_RET( at_cmd_tmpl, ... ) \
  do { \
    int M_err = _isbd_send_tiny_cmd( at_cmd_tmpl, __VA_ARGS__ ); \
    if ( M_err != ISBD_OK ) { return M_err; } \
  } while( 0 );


struct isbd {
  int err;
  struct at_uart at_uart;
  struct isbd_config config;
};

static struct isbd g_isbd = {  };

static at_uart_err_t _isbd_pack_bin_resp(
  uint8_t *msg_buf, uint16_t *msg_buf_len, uint16_t *csum, uint16_t timeout_ms 
);

static isbd_err_t _isbd_send_tiny_cmd( const char *at_cmd_tmpl, ... );


isbd_err_t isbd_setup( struct isbd_config *isbd_config ) {
  g_isbd.config = *isbd_config;

  at_uart_err_t ret = at_uart_setup( 
    &g_isbd.at_uart, &isbd_config->at_uart );

  return ret == AT_UART_OK ? ISBD_OK : ISBD_ERR_SETUP;
}


isbd_err_t isbd_get_imei( char *imei_buf, size_t imei_buf_len ) {
  
  ISBD_SEND_TINY_CMD_OR_RET( AT_CMD_TMPL_EXEC, "+cgsn" );

  g_isbd.err = at_uart_pack_txt_resp(
    &g_isbd.at_uart, imei_buf, imei_buf_len, AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  return g_isbd.err == AT_UART_OK ? ISBD_OK : ISBD_ERR_AT; 
}

isbd_err_t isbd_get_revision( char *rev_buf, size_t rev_buf_len ) {
  
  ISBD_SEND_TINY_CMD_OR_RET( AT_CMD_TMPL_EXEC, "+cgmr" );

  g_isbd.err = at_uart_pack_txt_resp( 
    // TODO: Measure exact lines from AT+CGMR
    &g_isbd.at_uart, rev_buf, rev_buf_len, 10, SHORT_TIMEOUT_RESPONSE );

  return g_isbd.err == AT_UART_OK ? ISBD_OK : ISBD_ERR_AT;
}

isbd_err_t isbd_get_rtc( char *rtc_buf, size_t rtc_buf_len ) {
  
  ISBD_SEND_TINY_CMD_OR_RET( AT_CMD_TMPL_EXEC, "+cclk" );

  g_isbd.err = at_uart_pack_txt_resp( 
    &g_isbd.at_uart, rtc_buf, rtc_buf_len, AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  return g_isbd.err == AT_UART_OK ? ISBD_OK : ISBD_ERR_AT;
}

isbd_err_t isbd_init_session( isbd_session_ext_t *session ) {
  
  ISBD_SEND_TINY_CMD_OR_RET( AT_CMD_TMPL_EXEC, "+sbdix" );

  char buf[ 64 ];
  g_isbd.err = at_uart_pack_txt_resp(
    &g_isbd.at_uart, buf, sizeof( buf ), AT_2_LINE_RESP, LONG_TIMEOUT_RESPONSE );
  
  if ( g_isbd.err == AT_UART_OK ) {

    // TODO: implement optimized function instead of using sscanf
    int read = sscanf( buf, "+SBDIX:%hhu,%hu,%hhu,%hu,%hu,%hhu",
      &session->mo_sts,
      &session->mo_msn,
      &session->mt_sts,
      &session->mt_msn,
      &session->mt_len,
      &session->mt_queued );

    return read == 6 ? ISBD_OK : ISBD_ERR;
  } 

  return ISBD_ERR_AT;
}

isbd_err_t isbd_clear_buffer( isbd_clear_buffer_t buffer ) {

  ISBD_SEND_TINY_CMD_OR_RET( AT_CMD_TMPL_EXEC_INT, "+sbdd", buffer );

  int err;
  uint8_t code;
  char str_code[ 16 ];

  // retrieve command response code
  // this is not AT command interface result code
  err = at_uart_get_resp_code(
    &g_isbd.at_uart, str_code, sizeof( str_code ), &code, SHORT_TIMEOUT_RESPONSE );

  if ( err == AT_UART_OK ) {
    err = at_uart_skip_txt_resp( 
      &g_isbd.at_uart, AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
  }

  if ( err == AT_UART_OK ) {
    g_isbd.err = code;
    return g_isbd.err == 0 ? ISBD_OK : ISBD_ERR;
  } else {
    g_isbd.err = err;
    return ISBD_ERR_AT;
  }

}


isbd_err_t isbd_set_mo_txt( const char *txt ) {

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
 
  ISBD_SEND_TINY_CMD_OR_RET( 
    AT_CMD_TMPL_SET_STR, "+sbdwt", txt );

  g_isbd.err = at_uart_skip_txt_resp( 
    &g_isbd.at_uart, AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  return g_isbd.err == AT_UART_OK ? ISBD_OK : ISBD_ERR_AT;
}

uint16_t isbd_compute_checksum( const uint8_t *msg_buf, uint16_t msg_buf_len ) {
  uint32_t sum = 0;
  for ( uint16_t i = 0; i < msg_buf_len; i++ ) {
    sum += msg_buf[ i ];
  }  
  return (sum & 0xFFFF);
}

isbd_err_t isbd_set_mo( const uint8_t *msg_buf, uint16_t msg_buf_len ) {

  // TODO: __data buffer is not mandatory and should be
  // TODO: removed in future modifications
  // uint8_t tx_buf_size = msg_len + 2;
  // uint8_t tx_buf[ tx_buf_size ];

  // unsigned char msg_len_buf[ 8 ];
  // snprintf( msg_len_buf, sizeof( msg_len_buf ), "%d", msg_len );

  ISBD_SEND_TINY_CMD_OR_RET( 
    AT_CMD_TMPL_SET_INT, "+sbdwb", msg_buf_len );
  
  // After the initial AT+SBDWB command
  // the ISU should answer with a READY string
  // but if the length is not correct 
  // the resulting value will be a code corresponding to 
  // the command context and not to the AT command interface itself
  int err;
  uint8_t code;
  char str_code[ 16 ];

  err = at_uart_get_resp_code( 
    &g_isbd.at_uart, str_code, sizeof( str_code ), &code, SHORT_TIMEOUT_RESPONSE );

  if ( err == AT_UART_UNK 
      && streq( str_code, AT_READY_STR ) ) {
      
    uint8_t csum_buf[ 2 ];
    *( (uint16_t*)&csum_buf[ 0 ] ) = htons( 
      isbd_compute_checksum( msg_buf, msg_buf_len ) );

    // finally write binary data to the ISU
    // MSG (N bytes) + CHECKSUM (2 bytes)
    zuart_write( // TODO: this should be an inline function named at_uart_write()
      &g_isbd.at_uart.zuart, msg_buf, msg_buf_len, SHORT_TIMEOUT_RESPONSE );

    zuart_write(
      &g_isbd.at_uart.zuart, csum_buf, sizeof( csum_buf ), SHORT_TIMEOUT_RESPONSE );

    // due to AT nature it is not strictly necessary to check this result
    // this is will be done in the last call of at_uart_skip_txt_resp()
    at_uart_get_resp_code(
      &g_isbd.at_uart, str_code, sizeof( str_code ), &code, SHORT_TIMEOUT_RESPONSE );

  }

  // fetch last AT code ( OK / ERR )
  err = at_uart_skip_txt_resp(
    &g_isbd.at_uart, AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  if ( err == AT_UART_OK ) {
    g_isbd.err = code;
    return code == 0 ? ISBD_OK : ISBD_ERR;
  } else {
    g_isbd.err = err;
    return ISBD_ERR_AT;
  }

}

isbd_err_t isbd_get_mt( uint8_t *__msg, uint16_t *msg_len, uint16_t *csum ) {
  
  ISBD_SEND_TINY_CMD_OR_RET( AT_CMD_TMPL_EXEC, "+sbdrb" );

  g_isbd.err = _isbd_pack_bin_resp(
    __msg, msg_len, csum, SHORT_TIMEOUT_RESPONSE );

  return g_isbd.err == AT_UART_OK ? ISBD_OK : ISBD_ERR_AT;
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

isbd_err_t isbd_mo_to_mt( char *out, uint16_t out_len ) {
  
  ISBD_SEND_TINY_CMD_OR_RET( AT_CMD_TMPL_EXEC, "+sbdtc" );
  
  g_isbd.err = at_uart_pack_txt_resp(
    &g_isbd.at_uart, out, out_len, AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
  
  return g_isbd.err == AT_UART_OK ? ISBD_OK : ISBD_ERR_AT;
}

// TODO: review and improve this function
isbd_err_t isbd_get_mt_txt( char *__mt_buff, size_t mt_buff_len ) {
  
  return ISBD_ERR;

  // at_uart_err_t ret;
  // ISBD_SEND_TINY_CMD_OR_RET( AT_CMD_TMPL_EXEC, "+sbdrt" );
  
  // if ( g_isbd.config.at_uart.verbose ) {  
  //   g_isbd.err = at_uart_pack_txt_resp( 
  //     &g_isbd.at_uart,__mt_buff, mt_buff_len, AT_3_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
  // } else {
    
  //   uint16_t len;
    
  //   at_uart_skip_txt_resp( 
  //     &g_isbd.at_uart, AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
    
  //   at_uart_pack_txt_resp( 
  //     &g_isbd.at_uart, __mt_buff, mt_buff_len, AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
    
  //   len = strlen( __mt_buff ) - 1;
    
  //   __mt_buff[ len ] = 0;
  //   int8_t code = atoi( &__mt_buff[ len ] ); 
    
  //   return code;
  // }

}

isbd_err_t isbd_get_sig_q( uint8_t *signal_q ) {

  ISBD_SEND_TINY_CMD_OR_RET( AT_CMD_TMPL_EXEC, "+csq" );
  
  int err;
  char buf[ 16 ];
  
  err = at_uart_pack_txt_resp(
    &g_isbd.at_uart, buf, sizeof( buf ), AT_2_LINE_RESP, LONG_TIMEOUT_RESPONSE );

  if ( err == AT_UART_OK ) {
    
    int read = sscanf( buf, "+CSQ:%hhd", signal_q );
    return read == 1 ? ISBD_OK : ISBD_ERR;

  } else {
    g_isbd.err = err;
    return ISBD_ERR_AT;
  }

}

isbd_err_t isbd_set_evt_report( isbd_evt_report_t *evt_report ) {

  char buf[ 16 ];

  snprintf( buf, sizeof( buf ), "%hhd,%hhd,%hhd", 
    evt_report->mode, evt_report->signal, evt_report->service );

  ISBD_SEND_TINY_CMD_OR_RET( AT_CMD_TMPL_SET_STR, "+cier", buf );

  // ! This command has a peculiarity,
  // ! after the command is successfully executed, it returns an OK response,
  // ! but just after that it transmits the first indicator event
  // ! so we skip those lines
  g_isbd.err = at_uart_skip_txt_resp( 
    &g_isbd.at_uart, AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );  

  if ( g_isbd.err == AT_UART_OK ) {
    
    uint8_t lines_to_skip = // [1,0] * ( [1,0] + [1,0] )
      evt_report->mode * ( evt_report->service + evt_report->signal );

    // TODO: we could return the resulting values instead of ignoring them ...
    if ( lines_to_skip > 0 ) {
      at_uart_skip_txt_resp( 
        &g_isbd.at_uart, lines_to_skip, SHORT_TIMEOUT_RESPONSE );
    }

  }

  return g_isbd.err == AT_UART_OK ? ISBD_OK : ISBD_ERR_AT;

}

static at_uart_err_t _isbd_pack_bin_resp(
  // TODO: timeout could be implicitly specified
  uint8_t *msg_buf, uint16_t *msg_buf_len, uint16_t *csum, uint16_t timeout_ms
) {

  // TODO: this should be at_uart_get_n_bytes ...
  zuart_read( 
    &g_isbd.at_uart.zuart, (uint8_t*)msg_buf_len, 2, timeout_ms ); // message length
  
  *msg_buf_len = ntohs( *msg_buf_len );

  zuart_read( 
    &g_isbd.at_uart.zuart, msg_buf, *msg_buf_len, timeout_ms );

  zuart_read( 
    &g_isbd.at_uart.zuart, (uint8_t*)csum, 2, timeout_ms );
  
  *csum = ntohs( *csum );

  return at_uart_skip_txt_resp( 
    &g_isbd.at_uart, AT_1_LINE_RESP, timeout_ms );
}

static isbd_err_t _isbd_send_tiny_cmd( const char *at_cmd_tmpl, ... ) {

  va_list args;
  va_start( args, at_cmd_tmpl );

  AT_DEFINE_CMD_BUFF( at_buf );

  at_uart_err_t err = at_uart_send_vcmd( 
    &g_isbd.at_uart, at_buf, sizeof( at_buf ), at_cmd_tmpl, args );

  va_end( args );

  g_isbd.err = err;

  return err == AT_UART_OK ? ISBD_OK : ISBD_ERR_AT;
}

int isbd_get_err() {
  return g_isbd.err;
}