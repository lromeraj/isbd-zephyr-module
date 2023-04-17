#include "stru.h"

#include "isu/dte.h"
#include "isu/evt.h"

#define VCODE_RING_STR    "SBDRING"
#define CODE_RING_STR     "126"

static inline bool _evt_parse_areg( const char *buf, isu_dte_evt_t *evt );
static inline bool _evt_parse_ciev( const char *buf, isu_dte_evt_t *evt );
static inline bool _evt_parse_ring( const char *buf, isu_dte_evt_t *evt, bool verbose );

isu_dte_err_t isbd_dte_evt_wait( isu_dte_t *dte, isu_dte_evt_t *event, uint32_t timeout_ms ) {

  char buf[ 32 ];
  event->id = ISU_DTE_EVT_UNK;

  dte->err = at_uart_pack_txt_resp(
    &dte->at_uart, buf, sizeof( buf ), AT_1_LINE_RESP, timeout_ms );

  bool verbose = dte->at_uart.config.verbose;

  if ( dte->err == AT_UART_UNK ) {

    if ( _evt_parse_ciev( buf, event )
      || _evt_parse_areg( buf, event ) 
      || _evt_parse_ring( buf, event, verbose ) ) {
      
      return ISU_DTE_OK;
    }

    return ISU_DTE_ERR_UNK;
  }

  // We expect AT to return unknown,
  // if the result code is AT_UART_OK means that the response
  // was a literal OK string and we are not expecting that ...
  return dte->err == AT_UART_OK
    ? ISU_DTE_ERR_UNK
    : ISU_DTE_ERR_AT;

}

static inline bool _evt_parse_ciev( const char *buf, isu_dte_evt_t *evt ) {

  uint8_t ciev_evt, ciev_val;

  evt->id = ISU_DTE_EVT_UNK;

  int ret = sscanf( buf, "+CIEV:%hhu,%hhu",
    &ciev_evt, &ciev_val );

  if ( ret == 2 ) {
    
    if ( ciev_evt == 0 ) {
      evt->id = ISU_DTE_EVT_SIGQ;
      evt->sigq = ciev_val;
    } else if ( ciev_evt == 1 ) {
      evt->id = ISU_DTE_EVT_SVCA;
      evt->svca = ciev_val;
    }
  
  }

  return evt->id != ISU_DTE_EVT_UNK;
}

static inline bool _evt_parse_ring( const char *buf, isu_dte_evt_t *evt, bool verbose ) {

  evt->id = ISU_DTE_EVT_UNK;
  
  const char *ring_str = 
    verbose ? VCODE_RING_STR : CODE_RING_STR;
  
  if ( streq( buf, ring_str ) ) {
    evt->id = ISU_DTE_EVT_RING;
    return true;
  }

  return false;
}

static inline bool _evt_parse_areg( const char *buf, isu_dte_evt_t *evt ) {

  evt->id = ISU_DTE_EVT_UNK;

  uint8_t areg_evt, areg_err;
  int ret = sscanf( buf, "+AREG:%hhu,%hhu",
    &areg_evt, &areg_err );

  if ( ret == 2 ) {
    evt->id = ISU_DTE_EVT_AREG;
    evt->areg.evt = areg_evt;
    evt->areg.err = areg_err;
    return true;
  }

  return false;
}