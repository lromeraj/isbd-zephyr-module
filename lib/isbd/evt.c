#include "stru.h"

#include "inc/isbd.h"
#include "inc/isbd/evt.h"

#define VCODE_RING_STR    "SBDRING"
#define CODE_RING_STR     "126"

static inline bool _evt_parse_areg( const char *buf, isbd_evt_t *evt );
static inline bool _evt_parse_ciev( const char *buf, isbd_evt_t *evt );
static inline bool _evt_parse_ring( isbd_t *isbd, const char *buf, isbd_evt_t *evt );

isbd_err_t isbd_evt_wait( isbd_t *isbd, isbd_evt_t *event, uint32_t timeout_ms ) {

  char buf[ 32 ];
  event->name = ISBD_EVENT_UNK;

  isbd->err = at_uart_pack_txt_resp(
    &isbd->at_uart, buf, sizeof( buf ), AT_1_LINE_RESP, timeout_ms );

  if ( isbd->err == AT_UART_UNK ) {

    if ( _evt_parse_ciev( buf, event )
      || _evt_parse_areg( buf, event ) 
      || _evt_parse_ring( isbd, buf, event ) ) {
      
      return ISBD_OK;
    }

    return ISBD_ERR_UNK;
  }

  // We expect AT to return unknown,
  // if the result code is AT_UART_OK means that the response
  // was a literal OK string and we are not expecting that ...
  return isbd->err == AT_UART_OK
    ? ISBD_ERR_UNK
    : ISBD_ERR_AT;

}

static inline bool _evt_parse_ciev( const char *buf, isbd_evt_t *evt ) {

  uint8_t ciev_evt, ciev_val;

  evt->name = ISBD_EVENT_UNK;

  int ret = sscanf( buf, "+CIEV:%hhu,%hhu", 
    &ciev_evt, &ciev_val );

  if ( ret == 2 ) {
    
    if ( ciev_evt == 0 ) {
      evt->name = ISBD_EVENT_SIGQ;
      evt->data.sigq = ciev_val;
    } else if ( ciev_evt == 1 ) {
      evt->name = ISBD_EVENT_SVCA;
      evt->data.svca = ciev_val;
    }
  
  }

  return evt->name != ISBD_EVENT_UNK;
}

static inline bool _evt_parse_ring( isbd_t *isbd, const char *buf, isbd_evt_t *evt ) {

  evt->name = ISBD_EVENT_UNK;
  
  const char *ring_str = isbd->at_uart.config.verbose 
    ? VCODE_RING_STR 
    : CODE_RING_STR;
  
  if ( streq( buf, ring_str ) ) {
    evt->name = ISBD_EVENT_RING;
    return true;
  }

  return false;
}

static inline bool _evt_parse_areg( const char *buf, isbd_evt_t *evt ) {

  evt->name = ISBD_EVENT_UNK;

  uint8_t areg_evt, areg_err;
  int ret = sscanf( buf, "+AREG:%hhu,%hhu", 
    &areg_evt, &areg_err );

  if ( ret == 2 ) {
    evt->name = ISBD_EVENT_AREG;
    evt->data.areg.evt = areg_evt;
    evt->data.areg.err = areg_err;
    return true;
  }

  return false;
}