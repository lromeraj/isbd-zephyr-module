#ifndef ISBD_MSG_H_ 
  #define ISBD_MSG_H_

  #include <stdint.h>

  #include "isu/dte.h"
  #include "isu/evt.h"

  struct isbd_mo_msg {
    uint16_t sn;
    uint8_t retries;
    uint8_t *data;
    uint16_t len;
  };

  struct isbd_mt_msg {
    uint16_t sn;
    uint8_t *data;
    uint16_t len;
  };

  typedef enum isbd_err {
    ISBD_OK, // everything was ok
    ISBD_ERR_UNK, // unknown error
    ISBD_ERR_MT, // could not read mt message
    ISBD_ERR_MO, // could not send MO message
    ISBD_ERR_MEM, // not enough memory
    ISBD_ERR_SPACE, // not enough space available
  } isbd_err_t;

  typedef enum isbd_evt_id {
    ISBD_EVT_MT,
    ISBD_EVT_MO,
    ISBD_EVT_DTE,
    ISBD_EVT_ERR,
  } isbd_evt_id_t;

  typedef struct isbd_evt {

    isbd_evt_id_t id;

    union {
      isbd_err_t err;
      isu_dte_evt_t dte;
      struct isbd_mt_msg mt;
      struct isbd_mo_msg mo;
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

  void isbd_destroy_evt( isbd_evt_t *evt );
  void isbd_destroy_mo_msg( struct isbd_mo_msg *mo_msg );
  void isbd_destroy_mt_msg( struct isbd_mt_msg *mt_msg );
  
  bool isbd_wait_evt( isbd_evt_t *isbd_evt, uint32_t timeout_ms );

  const char* isbd_err_name( isbd_err_t err );

#endif
