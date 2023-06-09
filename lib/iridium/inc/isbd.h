#ifndef ISBD_MSG_H_ 
  #define ISBD_MSG_H_

  #include <stdint.h>

  #include "isu/dte.h"
  #include "isu/evt.h"

  #define ISBD_DEFAULT_CONF( _dte ) \
    { \
      .dte = _dte, \
      .priority = 0, \
      .mo_queue_len = 4, \
      .evt_queue_len = 4, \
      .sigq_threshold = 2, \
    }

  struct isbd_mo_msg {
    bool alert; 
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
    ISBD_EVT_RING,
    ISBD_EVT_SVCA,
    ISBD_EVT_SIGQ,
    ISBD_EVT_ERR,
    ISBD_EVT_UNK,
  } isbd_evt_id_t;

  typedef struct isbd_evt {

    isbd_evt_id_t id;

    union {
      uint8_t svca;
      uint8_t sigq;
      isbd_err_t err;
      struct isbd_mt_msg mt;
      struct isbd_mo_msg mo;
    };

  } isbd_evt_t;

  typedef struct isbd_config {
    int priority;
    uint8_t sigq_threshold;
    uint8_t mo_queue_len;
    uint8_t evt_queue_len;
    isu_dte_t *dte;
  } isbd_config_t;

  isbd_err_t isbd_setup( isbd_config_t *isbd_conf );
  isbd_err_t isbd_send_mo_msg( const uint8_t *msg, uint16_t msg_len, uint8_t retries );

  /**
   * @brief Request a session
   * 
   * @note If there is currently any message in the mobile originated queue 
   * (or any session request) no session request will be enqueued. 
   * 
   * @param alert Flag to indicate if the session was requested 
   * due to a previously received ring alert
   */
  isbd_err_t isbd_request_session( bool alert );

  isbd_err_t isbd_destroy_evt( isbd_evt_t *evt );
  
  isbd_err_t isbd_destroy_mo_msg( struct isbd_mo_msg *mo_msg );
  isbd_err_t isbd_destroy_mt_msg( struct isbd_mt_msg *mt_msg );
  
  bool isbd_wait_evt( isbd_evt_t *isbd_evt, uint32_t timeout_ms );

  const char* isbd_err_name( isbd_err_t err );

#endif
