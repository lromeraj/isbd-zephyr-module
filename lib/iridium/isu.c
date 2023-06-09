#include <string.h>
#include <stdio.h>
#include <stdlib.h>
// #include <zephyr/kernel.h>
// #include <zephyr/device.h>
#include <zephyr/net/net_ip.h>
// #include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "stru.h"
#include "at_uart.h"

#include "isbd/util.h"

#include "isu.h"
#include "isu/dte.h"
#include "isu/evt.h"

LOG_MODULE_REGISTER( isu );

// codes prefixed with v means verbose version of the same code
#define VCODE_READY_STR        "READY"
#define CODE_READY_STR          VCODE_READY_STR

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

#define SEND_TINY_CMD_OR_RET( dte, at_cmd_tmpl, ... ) \
  do { \
    int M_err = isu_dte_send_tiny_cmd( dte, at_cmd_tmpl, __VA_ARGS__ ); \
    if ( M_err != ISU_DTE_OK ) { return M_err; } \
  } while( 0 );

static at_uart_err_t _unpack_bin_resp(
  isu_dte_t *dte, uint8_t *msg_buf, uint16_t *msg_buf_len, uint16_t *csum, uint16_t timeout_ms 
);

isu_dte_err_t isu_get_imei( isu_dte_t *dte, char *imei_buf, size_t imei_buf_len ) {
  
  SEND_TINY_CMD_OR_RET( dte, AT_CMD_TMPL_EXEC, "+CGSN" );

  dte->err = at_uart_parse_resp(
    &dte->at_uart, imei_buf, imei_buf_len, AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  return dte->err == AT_UART_OK ? ISU_DTE_OK : ISU_DTE_ERR_AT; 
}

isu_dte_err_t isu_get_revision( isu_dte_t *dte, char *rev_buf, size_t rev_buf_len ) {
  
  SEND_TINY_CMD_OR_RET( dte,  AT_CMD_TMPL_EXEC, "+CGMR" );

  dte->err = at_uart_parse_resp( 
    &dte->at_uart, rev_buf, rev_buf_len, AT_UNK_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  return dte->err == AT_UART_OK ? ISU_DTE_OK : ISU_DTE_ERR_AT;
}

isu_dte_err_t isu_get_rtc( isu_dte_t *dte, char *rtc_buf, size_t rtc_buf_len ) {
  
  SEND_TINY_CMD_OR_RET( dte, AT_CMD_TMPL_EXEC, "+CCLK" );

  dte->err = at_uart_parse_resp( 
    &dte->at_uart, rtc_buf, rtc_buf_len, AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  return dte->err == AT_UART_OK ? ISU_DTE_OK : ISU_DTE_ERR_AT;
}

// TODO: this should be named isbd_init_session_ext()
isu_dte_err_t isu_init_session( isu_dte_t *dte, isu_session_ext_t *session, bool alert ) {
  
  SEND_TINY_CMD_OR_RET( 
    dte, AT_CMD_TMPL_EXEC_STR, "+SBDIX", alert ? "A" : "" );

  char buf[ 64 ];
  dte->err = at_uart_parse_resp(
    &dte->at_uart, buf, sizeof( buf ), AT_2_LINE_RESP, LONG_TIMEOUT_RESPONSE );
  
  LOG_DBG( "%s", buf );

  if ( dte->err == AT_UART_OK ) {
    
    // TODO: implement optimized function instead of using sscanf
    int read = sscanf( buf, "+SBDIX:%hhu,%hu,%hhu,%hu,%hu,%hhu",
      &session->mo_sts,
      &session->mo_msn,
      &session->mt_sts,
      &session->mt_msn,
      &session->mt_len,
      &session->mt_queued );
    
    return read == 6 ? ISU_DTE_OK : ISU_DTE_ERR_UNK;

  } else {
    return ISU_DTE_ERR_AT;
  } 

}

isu_dte_err_t isu_clear_buffer( isu_dte_t *dte, isu_clear_buffer_t buffer ) {

  SEND_TINY_CMD_OR_RET( dte, AT_CMD_TMPL_EXEC_INT, "+SBDD", buffer );

  int err;
  uint8_t code;
  char str_code[ 16 ];

  // retrieve command response code
  // this is not AT command interface result code
  err = at_uart_get_resp_code(
    &dte->at_uart, str_code, sizeof( str_code ), &code, SHORT_TIMEOUT_RESPONSE );

  if ( err == AT_UART_OK ) {
    err = at_uart_skip_resp( 
      &dte->at_uart, AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
  }

  if ( err == AT_UART_OK ) {
    dte->err = code;
    return dte->err == 0 ? ISU_DTE_OK : ISU_DTE_ERR_CMD;
  } else {
    dte->err = err;
    return ISU_DTE_ERR_AT;
  }

}


isu_dte_err_t isu_set_mo_txt( isu_dte_t *dte, const char *txt ) {

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
 
  SEND_TINY_CMD_OR_RET( 
    dte, AT_CMD_TMPL_SET_STR, "+SBDWT", txt );

  dte->err = at_uart_skip_resp( 
    &dte->at_uart, AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  return dte->err == AT_UART_OK ? ISU_DTE_OK : ISU_DTE_ERR_AT;
}

isu_dte_err_t isu_set_mo( isu_dte_t *dte, const uint8_t *msg_buf, uint16_t msg_buf_len ) {

  SEND_TINY_CMD_OR_RET( 
    dte, AT_CMD_TMPL_SET_INT, "+SBDWB", msg_buf_len );
  
  // After the initial AT+SBDWB command
  // the ISU should answer with a READY string
  // but if the length is not correct 
  // the resulting value will be a code corresponding to 
  // the command context and not to the AT command interface itself
  
  int at_err;
  uint8_t code;
  char str_code[ 16 ];

  at_err = at_uart_get_resp_code( 
    &dte->at_uart, str_code, sizeof( str_code ), &code, SHORT_TIMEOUT_RESPONSE );

  if ( at_err == AT_UART_UNK 
      && streq( str_code, CODE_READY_STR ) ) {
      
    uint8_t csum_buf[ 2 ];

    *( (uint16_t*)&csum_buf[ 0 ] ) = 
      htons( isbd_util_compute_checksum( msg_buf, msg_buf_len ) );

    // finally write binary data to the ISU
    // MSG (N bytes) + CHECKSUM (2 bytes)
    at_uart_write( // TODO: if write fails return immediately
      &dte->at_uart, msg_buf, msg_buf_len, SHORT_TIMEOUT_RESPONSE );

    at_uart_write( // TODO: if write fails return immediately
      &dte->at_uart, csum_buf, sizeof( csum_buf ), SHORT_TIMEOUT_RESPONSE );

    // Due to AT nature it is not strictly necessary to check this result.
    // This is will be done in the last call of at_uart_skip_resp()
    at_uart_get_resp_code(
      &dte->at_uart, str_code, sizeof( str_code ), &code, SHORT_TIMEOUT_RESPONSE );

  }

  // fetch last AT code ( OK / ERR )
  at_err = at_uart_skip_resp(
    &dte->at_uart, AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  if ( at_err == AT_UART_OK ) {
    dte->err = code;
    return code == 0 ? ISU_DTE_OK : ISU_DTE_ERR_CMD;
  } else {
    dte->err = at_err;
    return ISU_DTE_ERR_AT;
  }

}

isu_dte_err_t isu_get_mt( isu_dte_t *dte, uint8_t *msg, uint16_t *msg_len, uint16_t *csum ) {
  
  SEND_TINY_CMD_OR_RET( 
    dte, AT_CMD_TMPL_EXEC, "+SBDRB" );

  dte->err = _unpack_bin_resp(
    dte, msg, msg_len, csum, SHORT_TIMEOUT_RESPONSE );
  
  if ( dte->err == AT_UART_OK ) {
    
    dte->err = at_uart_skip_resp( 
      &dte->at_uart, AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
  }

  return dte->err == AT_UART_OK 
    ? ISU_DTE_OK 
    : ISU_DTE_ERR_AT;
}

/*
int8_t isbd_set_mo_txt_l( char *__txt ) {
  
  SEND_AT_CMD_EXT( "sbdwt" );

  int8_t cmd_code;
  isbd_at_code_t at_code = 
    at_uart_parse_resp_code( &cmd_code, 100 );
  
  printk("AT CODE: %d\n", at_code );

  if ( at_code == ISBD_AT_RDY ) {

    // TODO: add \r char

    uint16_t txt_len = strlen( __txt );
    _uart_write( (uint8_t*)__txt, txt_len );

    at_code = at_uart_parse_resp( NULL, AT_2_LINE_RESP, 100 );
  }

  return cmd_code;
}
*/

isu_dte_err_t isu_mo_to_mt( isu_dte_t *dte, char *out, uint16_t out_len ) {
  
  SEND_TINY_CMD_OR_RET( 
    dte, AT_CMD_TMPL_EXEC, "+SBDTC" );
  
  dte->err = at_uart_parse_resp(
    &dte->at_uart, out, out_len, AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );
  
  return dte->err == AT_UART_OK ? ISU_DTE_OK : ISU_DTE_ERR_AT;
}

isu_dte_err_t isu_get_mt_txt( isu_dte_t *dte, char *mt_buf, size_t mt_buf_len ) {
  
  SEND_TINY_CMD_OR_RET( 
    dte, AT_CMD_TMPL_EXEC, "+SBDRT" );

  at_uart_skip_resp(
    &dte->at_uart, AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  dte->err = at_uart_parse_resp( 
    &dte->at_uart, mt_buf, mt_buf_len, AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  if ( dte->config.at_uart.verbose ) {  
    
    dte->err = at_uart_skip_resp( 
      &dte->at_uart, AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  } else if ( dte->err == AT_UART_UNK ) { 
    
    // This is necessary due to a Iridium implementation/design fault 
    // (not following correctly AT standard) specifically for this command
    // The problem consist in that when verbose mode is disabled
    // the AT result code '0' is concatenated with the text itself
    // so the response looks like the following:
    // > +SBDRT:\r\n
    // > My awesome message0\r
    // When it should be:
    // > +SBDRT:\r\n <-- This still conflicts with the specification in their manual
    // > My awesome message\r0\r
    // This behaviour is probably caused by the text nature of the command itself 
    // The delimiter \n is used to indicate the end of message when using AT+SBDWT
    // so the use of \r is theoretically valid as a value of the message itself
    // but is conflicts with the AT standard

    uint16_t len = strlen( mt_buf ) - 1;

    if ( mt_buf[ len ] == '0' ) {
      mt_buf[ len ] = 0;
      dte->err = AT_UART_OK;
    }

  }

  return dte->err == AT_UART_OK
    ? ISU_DTE_OK
    : ISU_DTE_ERR_AT;

}

isu_dte_err_t isu_get_sig_q( isu_dte_t *dte, uint8_t *signal_q ) {

  SEND_TINY_CMD_OR_RET( 
    dte, AT_CMD_TMPL_EXEC, "+CSQ" );
  
  char buf[ 16 ];
  
  dte->err = at_uart_parse_resp(
    &dte->at_uart, buf, sizeof( buf ), AT_2_LINE_RESP, LONG_TIMEOUT_RESPONSE );

  if ( dte->err == AT_UART_OK ) {
    int read = sscanf( buf, "+CSQ:%hhu", signal_q );
    return read == 1 ? ISU_DTE_OK : ISU_DTE_ERR_UNK;
  }

  return ISU_DTE_ERR_AT;

}

isu_dte_err_t isu_set_evt_report( 
  isu_dte_t *dte, isu_evt_report_t *evt_report, uint8_t *sigq, uint8_t *svca
) {

  char buf[ 16 ];

  snprintf( buf, sizeof( buf ), "%hhd,%hhd,%hhd", 
    evt_report->mode, evt_report->signal, evt_report->service );

  SEND_TINY_CMD_OR_RET( 
    dte, AT_CMD_TMPL_SET_STR, "+CIER", buf );

  at_uart_err_t at_err = at_uart_skip_resp( 
    &dte->at_uart, AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  // ! This command has a peculiarity,
  // ! after the command is successfully executed, it returns an OK response,
  // ! but just after that it transmits the first indicator events
  if ( at_err == AT_UART_OK ) {
      
    uint8_t events = // [1, 0] * ( [1, 0] + [1, 0] )
      evt_report->mode * ( evt_report->service + evt_report->signal );
    
    while ( events > 0 ) {

      isu_dte_evt_t evt;
      isu_dte_err_t dte_err = isu_dte_evt_wait(
        dte, &evt, SHORT_TIMEOUT_RESPONSE );

      if ( dte_err == ISU_DTE_OK ) {
        
        if ( evt.id == ISU_DTE_EVT_SIGQ && sigq ) {
          *sigq = evt.sigq;
        } else if ( evt.id == ISU_DTE_EVT_SVCA && svca ) {
          *svca = evt.svca;
        }

        events--;

      } else {
        return dte_err;
      }

    }

    return ISU_DTE_OK;

  } else {

    dte->err = at_err;
    return ISU_DTE_ERR_AT;
  }

}

isu_dte_err_t isu_set_mt_alert( isu_dte_t *dte, isu_mt_alert_t alert ) {

  SEND_TINY_CMD_OR_RET( 
    dte, AT_CMD_TMPL_SET_INT, "+SBDMTA", alert );
  
  dte->err = at_uart_skip_resp( 
    &dte->at_uart, AT_1_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  return dte->err == AT_UART_OK ? ISU_DTE_OK : ISU_DTE_ERR_AT;
}

isu_dte_err_t isu_get_mt_alert( isu_dte_t *dte, isu_mt_alert_t *alert ) {

  SEND_TINY_CMD_OR_RET( 
    dte, AT_CMD_TMPL_READ, "+SBDMTA" ); 


  char buf[ 32 ];
  dte->err = at_uart_parse_resp( 
    &dte->at_uart, buf, sizeof( buf ), AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  if ( dte->err == AT_UART_OK ) {

    uint8_t val;
    int read = sscanf( buf, "+SBDMTA:%hhu", &val );
    
    if ( read == 1 ) {
      *alert = val;
      return ISU_DTE_OK;
    }

    return ISU_DTE_ERR_UNK;

  } else {
    return ISU_DTE_ERR_AT;
  }

}

isu_dte_err_t isu_net_reg( isu_dte_t *dte, isu_net_reg_sts_t *out_sts ) {

  SEND_TINY_CMD_OR_RET( 
    dte, AT_CMD_TMPL_EXEC, "+SBDREG" );

  char buf[ 32 ];

  dte->err = at_uart_parse_resp( 
    &dte->at_uart, buf, sizeof( buf ), AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  if ( dte->err == AT_UART_OK ) {

    uint8_t status, code;
    int read = sscanf( buf, "+SBDREG:%hhu,%hhu", &status, &code );

    if ( read == 2 ) {

      dte->err = code;

      if ( out_sts ) *out_sts = status;

      return code == 0 ? ISU_DTE_OK : ISU_DTE_ERR_CMD;

    } else {
      return ISU_DTE_ERR_UNK;
    }

  } else {
    return ISU_DTE_ERR_AT;
  }

}

isu_dte_err_t isu_get_ring_sts( isu_dte_t *dte, isu_ring_sts_t *ring_sts ) {

  SEND_TINY_CMD_OR_RET( 
    dte, AT_CMD_TMPL_EXEC, "+CRIS" );

  uint8_t   tri, // indicates the telephony ring indication status
            sri; // indicates the SBD ring indication status

  char buf[ 32 ];
  dte->err = at_uart_parse_resp( 
    &dte->at_uart, buf, sizeof( buf ), AT_2_LINE_RESP, SHORT_TIMEOUT_RESPONSE );

  if ( dte->err == AT_UART_OK ) {
    int read = sscanf( buf, "+CRIS:%hhu,%hhu", &tri, &sri );

    *ring_sts = sri;
    return read == 2 ? ISU_DTE_OK : ISU_DTE_ERR_UNK;

  } else {
    return ISU_DTE_ERR_AT;
  }
}

static at_uart_err_t _unpack_bin_resp(
  isu_dte_t *dte, 
  uint8_t *msg_buf, uint16_t *msg_buf_len, 
  uint16_t *csum, uint16_t timeout_ms
) {

  uint16_t msg_len;
  at_uart_err_t ret = AT_UART_OK;
  
  // flag used to detect if the given buffer is smaller than necessary
  bool overflowed = false;

  ret = at_uart_read(
    &dte->at_uart, (uint8_t*) &msg_len, 2, timeout_ms ); // message length

  if ( ret == AT_UART_OK ) {
    
    msg_len = ntohs( msg_len );

    if ( msg_len > *msg_buf_len ) {
      // msg_buf = NULL; // skip data
      overflowed = true;
      LOG_ERR( "Overflow %hu > %hu\n", msg_len, *msg_buf_len );
    } else {
      *msg_buf_len = msg_len;
    }
    
    // ! This will probably block if received length is not valid
    // ! due to remanent chars or electrical noise ... 
    ret = at_uart_read(
      &dte->at_uart, msg_buf, *msg_buf_len, timeout_ms );

  }  

  if ( ret == AT_UART_OK ) {
    ret = at_uart_read(
      &dte->at_uart, (uint8_t*)csum, 2, timeout_ms );

    *csum = ntohs( *csum );
  }

  return overflowed ? AT_UART_OVERFLOW : ret;
}