#include "stru.h"

#include "isbd.h"
#include "isbd/evt.h"

#define VCODE_RING_STR    "SBDRING"
#define CODE_RING_STR     "126"

static inline bool _evt_parse_areg( const char *buf, isbd_evt_t *evt );
static inline bool _evt_parse_ciev( const char *buf, isbd_evt_t *evt );
static inline bool _evt_parse_ring( const char *buf, isbd_evt_t *evt, bool verbose );

isu_dte_err_t isbd_evt_wait( isu_dte_t *dte, isbd_evt_t *event, uint32_t timeout_ms ) {

  char buf[ 32 ];
  event->id = ISBD_EVENT_UNK;

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

static inline bool _evt_parse_ciev( const char *buf, isbd_evt_t *evt ) {

  uint8_t ciev_evt, ciev_val;

  evt->id = ISBD_EVENT_UNK;

  int ret = sscanf( buf, "+CIEV:%hhu,%hhu", 
    &ciev_evt, &ciev_val );

  if ( ret == 2 ) {
    
    if ( ciev_evt == 0 ) {
      evt->id = ISBD_EVENT_SIGQ;
      evt->data.sigq = ciev_val;
    } else if ( ciev_evt == 1 ) {
      evt->id = ISBD_EVENT_SVCA;
      evt->data.svca = ciev_val;
    }
  
  }

  return evt->id != ISBD_EVENT_UNK;
}

static inline bool _evt_parse_ring( const char *buf, isbd_evt_t *evt, bool verbose ) {

  evt->id = ISBD_EVENT_UNK;
  
  const char *ring_str = 
    verbose ? VCODE_RING_STR : CODE_RING_STR;
  
  if ( streq( buf, ring_str ) ) {
    evt->id = ISBD_EVENT_RING;
    return true;
  }

  return false;
}

static inline bool _evt_parse_areg( const char *buf, isbd_evt_t *evt ) {

  evt->id = ISBD_EVENT_UNK;

  uint8_t areg_evt, areg_err;
  int ret = sscanf( buf, "+AREG:%hhu,%hhu", 
    &areg_evt, &areg_err );

  if ( ret == 2 ) {
    evt->id = ISBD_EVENT_AREG;
    evt->data.areg.evt = areg_evt;
    evt->data.areg.err = areg_err;
    return true;
  }

  return false;
}