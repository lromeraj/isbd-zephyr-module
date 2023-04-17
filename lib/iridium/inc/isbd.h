#ifndef ISBD_MSG_H_ 
  #define ISBD_MSG_H_

  #include <stdint.h>

  #include "isu/dte.h"
  #include "isu/evt.h"

  typedef enum isbd_evt_id {
    ISBD_EVT_MT,
    ISBD_EVT_MO,
    ISBD_EVT_DTE,
  } isbd_evt_id_t;

  typedef struct isbd_evt {

    isbd_evt_id_t id;

    union {

      struct {
        uint16_t sn;
        uint16_t len;
        uint8_t *msg;
      } mt;

      struct {
        uint16_t sn;
        uint16_t len;
      } mo;

      isu_dte_evt_t dte;

    };

  } isbd_evt_t;

  typedef struct isbd_config {
    int priority;
    uint8_t mo_queue_len;
    uint8_t evt_queue_len;
    isu_dte_t *dte;
  } isbd_config_t;

  void isbd_setup( isbd_config_t *isbd_conf );
  void isbd_enqueue_mo_msg( const uint8_t *msg, uint16_t msg_len, uint8_t retries );
  void isbd_request_mt_msg();
  
  bool isbd_evt_wait( isbd_evt_t *isbd_evt, uint32_t timeout_ms );
#endif
