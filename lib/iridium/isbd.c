
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "isu.h"
#include "isbd.h"
#include "isbd/evt.h"
#include "isbd/msg.h"
#include "isbd/util.h"

#define DTE_EVT_WAIT_TIMEOUT    1000

#define DO_FOREVER while( 1 )

#define ISBD_DTE \
  g_isbd.cnf.dte

#define ISBD_MO_Q \
  &g_isbd.mo_msgq

#define ISBD_MT_Q \
  &g_isbd.mt_msgq

#define ISBD_EVT_Q \
  &g_isbd.evt_msgq
  
typedef struct isbd {
  char *mo_msgq_buf;
  char *mt_msgq_buf;
  char *evt_msgq_buf;
  struct k_msgq mo_msgq;
  struct k_msgq mt_msgq;
  struct k_msgq evt_msgq;
  isbd_config_t cnf;
} isbd_t;

struct mo_msg {
  uint8_t retries;
  uint8_t *data;
  uint16_t len;
};

extern void _entry_point( void *, void *, void * );

void _mo_msg_destroy( struct mo_msg *mo_msg );
void _mo_msg_requeue( struct mo_msg *mo_msg );

K_THREAD_STACK_DEFINE( 
  g_thread_stack_area, CONFIG_ISBD_THREAD_STACK_SIZE );

static struct isbd g_isbd;
static struct k_thread g_thread_data;

static bool _read_mt( uint8_t *buf, uint16_t *buf_len ) {

  uint16_t recv_csum;

  isu_dte_err_t ret = 
    isu_get_mt( ISBD_DTE, buf, buf_len, &recv_csum );

  if ( ret == ISU_DTE_OK ) {

    uint16_t host_csum = 
      isbd_util_compute_checksum( buf, *buf_len ); 
    
    return recv_csum == host_csum;
  }

  return false;

}

// TODO: refactor
void _init_session( struct mo_msg *mo_msg ) {

  isu_dte_err_t ret;

  if ( mo_msg->data && mo_msg->len > 0 ) {
    printk( "Sending message #%hu ...\n", mo_msg->len );
    ret = isu_set_mo( ISBD_DTE, mo_msg->data, mo_msg->len );
  } else {
    ret = isu_clear_buffer( ISBD_DTE, ISU_CLEAR_MO_BUFF );
  }

  if ( ret == ISU_DTE_OK ) {

    isu_session_ext_t session;
    ret = isu_init_session( ISBD_DTE, &session, false );

    if ( ret == ISU_DTE_OK ) {

      if ( mo_msg->data && mo_msg->len > 0 ) {
        
        if ( session.mo_sts < 3 ) {
          
          struct isbd_evt evt;
          
          evt.id = ISBD_EVT_MO;

          evt.mo.sn = session.mo_msn;
          evt.mo.len = mo_msg->len;
      
          k_msgq_put( ISBD_EVT_Q, &evt, K_NO_WAIT );
          
          _mo_msg_destroy( mo_msg );

        } else {

          if ( mo_msg->retries > 0 ) {
            mo_msg->retries--;
            _mo_msg_requeue( mo_msg );
          } else {
            _mo_msg_destroy( mo_msg );
          }

        }

      }

      if ( session.mt_sts == 1 ) {
        
        struct isbd_evt evt;

        evt.id = ISBD_EVT_MT;

        evt.mt.sn = session.mt_msn;
        evt.mt.len = session.mt_len;
        evt.mt.msg = (uint8_t*) k_malloc( sizeof( uint8_t ) * session.mt_len );

        if ( _read_mt( evt.mt.msg, &evt.mt.len ) ) {
          k_msgq_put( ISBD_EVT_Q, &evt, K_NO_WAIT );
        }

      }

    }

  }



}

void _entry_point( void *v1, void *v2, void *v3 ) {

  isu_evt_report_t evt_report = {
    .mode     = 1,
    .signal   = 1,
    .service  = 1,
  };

  // ! This function traps the first two immediate events
  // TODO: create an issue or Wiki entry to explain this "problem"
  isu_set_evt_report( ISBD_DTE, &evt_report );

  DO_FOREVER {

    struct mo_msg mo_msg;

    if ( k_msgq_get( ISBD_MO_Q, &mo_msg, K_NO_WAIT ) == 0 ) {
      _init_session( &mo_msg );
    }

    isbd_dte_evt_t dte_evt;

    isu_dte_err_t dte_err = isbd_dte_evt_wait( 
      ISBD_DTE, &dte_evt, DTE_EVT_WAIT_TIMEOUT );

    if ( dte_err == ISU_DTE_OK ) {

      isbd_evt_t isbd_evt;

      isbd_evt.id = ISBD_EVT_DTE;
      isbd_evt.dte = dte_evt;

      k_msgq_put( ISBD_EVT_Q, &isbd_evt, K_NO_WAIT );
    }

  }

}

void _mo_msg_destroy( struct mo_msg *mo_msg ) {

  if ( mo_msg->data ) {
    k_free( mo_msg->data );
  }

  mo_msg->len = 0;
  mo_msg->data = NULL;
  mo_msg->retries = 0;
}

void _mo_msg_requeue( struct mo_msg *mo_msg ) {
  if ( k_msgq_put( ISBD_MO_Q, mo_msg, K_NO_WAIT ) == 0 ) {
    printk( "Message #%hu was re-enqueued\n", mo_msg->len );
  }
}

/**
 * @brief Enqueues a message to be sent.
 * 
 * @param msg Message buffer
 * @param msg_len Message buffer length
 */
void isbd_enqueue_mo_msg( const uint8_t *msg, uint16_t msg_len, uint8_t retries ) {

  struct mo_msg mo_msg;
  
  mo_msg.retries = retries;

  if ( msg_len > 0 && msg ) {
    mo_msg.len = msg_len;
    mo_msg.data = (uint8_t*)k_malloc( sizeof(uint8_t) * msg_len );
    memcpy( mo_msg.data, msg, msg_len );
  } else {
    mo_msg.len = 0;
    mo_msg.data = NULL;
  }

  if ( k_msgq_put( ISBD_MO_Q, &mo_msg, K_NO_WAIT ) == 0 ) {
    printk( "Message #%hu was enqueued\n", msg_len );
  }
  
}

void isbd_request_mt_msg() {
  // if the queue already has pending session requests
  // there is no need to push an other one
  if ( k_msgq_num_used_get( ISBD_MO_Q ) == 0 ) {
    isbd_enqueue_mo_msg( NULL, 0, 0 );
  }
}

void isbd_setup( isbd_config_t *isbd_conf ) {

  g_isbd.cnf = *isbd_conf;

  g_isbd.mo_msgq_buf = 
    (char*) k_malloc( sizeof( struct mo_msg ) * g_isbd.cnf.mo_queue_len );

  // g_isbd.mt_msgq_buf = 
  //   (char*) k_malloc( sizeof( struct msgq_item ) * g_isbd.cnf.mt_queue_len );

  g_isbd.evt_msgq_buf = 
    (char*) k_malloc( sizeof( struct isbd_evt ) * g_isbd.cnf.evt_queue_len );

  k_msgq_init( 
    ISBD_MO_Q,
    g_isbd.mo_msgq_buf, 
    sizeof( struct mo_msg ), 
    g_isbd.cnf.mo_queue_len );

  // k_msgq_init( 
  //   ISBD_MT_Q, 
  //   g_isbd.mt_msgq_buf, 
  //   sizeof( struct msgq_item ), 
  //   g_isbd.cnf.mt_queue_len );

  k_msgq_init(
    ISBD_EVT_Q,
    g_isbd.evt_msgq_buf,
    sizeof( struct isbd_evt ),
    g_isbd.cnf.evt_queue_len );

  // k_tid_t my_tid = k_thread_create( 
  k_thread_create( 
    &g_thread_data, g_thread_stack_area,
    K_THREAD_STACK_SIZEOF( g_thread_stack_area ),
    _entry_point,
    NULL, NULL, NULL,
    g_isbd.cnf.priority, 0, K_NO_WAIT );

}

bool isbd_evt_wait( isbd_evt_t *isbd_evt, uint32_t timeout_ms ) {
  return k_msgq_get( ISBD_EVT_Q, isbd_evt, K_MSEC( timeout_ms ) ) == 0;
}