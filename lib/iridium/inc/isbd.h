#ifndef ISBD_MSG_H_ 
  #define ISBD_MSG_H_

  #include <stdint.h>
  
  #include "isu/dte.h"

  typedef struct isbd_config {
    isu_dte_t *dte;
    int priority;
    uint8_t mo_queue_len;
    uint8_t mt_queue_len;
  } isbd_config_t;

  void isbd_setup( isbd_config_t *isbd_conf );
  void isbd_msg_enqueue( const uint8_t *msg, uint16_t msg_len, uint8_t retries );

#endif