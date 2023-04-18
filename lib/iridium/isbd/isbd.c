
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "isu.h"
#include "isu/evt.h"

#include "isbd.h"
#include "isbd/util.h"

LOG_MODULE_REGISTER( isbd );

#define DTE_EVT_WAIT_TIMEOUT    5000

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
  uint8_t svca; // service availability
  char *mo_msgq_buf;
  char *mt_msgq_buf;
  char *evt_msgq_buf;
  struct k_msgq mo_msgq;
  struct k_msgq mt_msgq;
  struct k_msgq evt_msgq;
  isbd_config_t cnf;
} isbd_t;

extern void _entry_point( void *, void *, void * );
static inline void _requeue_mo_msg( struct isbd_mo_msg *mo_msg );

K_THREAD_STACK_DEFINE(
  g_thread_stack_area, CONFIG_ISBD_THREAD_STACK_SIZE );

static struct isbd g_isbd;
static struct k_thread g_thread_data;

static inline bool _read_mt_msg( uint8_t *buf, uint16_t *buf_len ) {

  uint16_t recv_csum;

  isu_dte_err_t ret = 
    isu_get_mt( ISBD_DTE, buf, buf_len, &recv_csum );

  if ( ret == ISU_DTE_OK ) {
    
    uint16_t host_csum =
      isbd_util_compute_checksum( buf, *buf_len );
    
    return recv_csum == host_csum;

  } else {
    LOG_DBG( "Could not get MT message (%03d) -> (%03d)", 
      ret, isu_dte_get_err( ISBD_DTE ) );
  }

  return false;
}

static inline void _notify_err( isbd_err_t err ) {

  isbd_evt_t evt;

  evt.id = ISBD_EVT_ERR;
  evt.err = err;

  k_msgq_put( ISBD_EVT_Q, &evt, K_NO_WAIT );

}

static inline void _notify_mt_msg( struct isbd_mt_msg *mt_msg ) {

  isbd_evt_t evt;
  
  evt.id = ISBD_EVT_MT;
  evt.mt = *mt_msg;


  if ( k_msgq_put( ISBD_EVT_Q, &evt, K_NO_WAIT ) != 0 ) {
    isbd_destroy_mt_msg( mt_msg );
  }

}

static inline void _notify_mo_msg( struct isbd_mo_msg *mo_msg ) {
  
  isbd_evt_t evt;
  
  evt.id = ISBD_EVT_MO;
  evt.mo = *mo_msg;

  if ( k_msgq_put( ISBD_EVT_Q, &evt, K_NO_WAIT ) != 0 ) {
    isbd_destroy_mo_msg( mo_msg );
  }

}

static inline void _handle_session_mo_msg( 
  isu_session_ext_t *session, struct isbd_mo_msg *mo_msg 
) {

  if ( mo_msg->data && mo_msg->len > 0 ) {
    
    if ( session->mo_sts < 3 ) {
    
      mo_msg->sn = session->mo_msn;
      _notify_mo_msg( mo_msg );

    } else {

      if ( mo_msg->retries > 0 ) {
        mo_msg->retries--;
        _requeue_mo_msg( mo_msg );
      } else {
        _notify_err( ISBD_ERR_MO );
        isbd_destroy_mo_msg( mo_msg );
      }

    }

  }

}

static inline void _handle_session_mt_msg(
  isu_session_ext_t *session
) {

  if ( session->mt_sts == 1 ) {

    struct isbd_mt_msg mt_msg;

    mt_msg.sn = session->mt_msn;
    mt_msg.len = session->mt_len;
    mt_msg.data = (uint8_t*) k_malloc( sizeof( uint8_t ) * session->mt_len );
    
    if ( mt_msg.data ) {

      LOG_DBG( "Reading MT message, len=%hu", mt_msg.len );

      bool msg_read = 
        _read_mt_msg( mt_msg.data, &mt_msg.len );

      if ( msg_read ) {
        _notify_mt_msg( &mt_msg );
      } else {
        _notify_err( ISBD_ERR_MT );
        isbd_destroy_mt_msg( &mt_msg );
      }

    } else {
      // TODO: notify error
    }

  }

}

void _init_session( struct isbd_mo_msg *mo_msg ) {

  isu_dte_err_t ret;

  if ( mo_msg->data && mo_msg->len > 0 ) {
    LOG_DBG( "_init_session() - Setting MO message len=%hu", mo_msg->len );
    ret = isu_set_mo( ISBD_DTE, mo_msg->data, mo_msg->len );
  } else {
    LOG_DBG( "_init_session() - Clearing MO message" );
    ret = isu_clear_buffer( ISBD_DTE, ISU_CLEAR_MO_BUFF );
  }

  if ( ret == ISU_DTE_OK ) {

    isu_session_ext_t session;
    ret = isu_init_session( ISBD_DTE, &session, false );

    if ( ret == ISU_DTE_OK ) {
      _handle_session_mo_msg( &session, mo_msg );
      _handle_session_mt_msg( &session );
    }

  }

}

void isbd_destroy_mt_msg( struct isbd_mt_msg *mt_msg ) {
  if ( mt_msg->data ) {
    k_free( mt_msg->data );
  }
  mt_msg->len = 0;
}

void _entry_point( void *v1, void *v2, void *v3 ) {

  isu_evt_report_t evt_report = {
    .mode     = 1,
    .signal   = 1,
    .service  = 1,
  };

  isu_dte_err_t dte_err;

  isu_set_evt_report(
    ISBD_DTE, &evt_report, NULL, &g_isbd.svca );

  isu_set_mt_alert( ISBD_DTE, ISU_MT_ALERT_ENABLED );

  DO_FOREVER {

    struct isbd_mo_msg mo_msg;
    
    // sessions will be sent only if the service is currently available
    if ( g_isbd.svca ) {
      if ( k_msgq_get( ISBD_MO_Q, &mo_msg, K_NO_WAIT ) == 0 ) {
        _init_session( &mo_msg );
      }
    }

    isu_dte_evt_t dte_evt;

    dte_err = isu_dte_evt_wait(
      ISBD_DTE, &dte_evt, DTE_EVT_WAIT_TIMEOUT );

    if ( dte_err == ISU_DTE_OK ) {
      
      if ( dte_evt.id == ISU_DTE_EVT_SVCA ) {
        g_isbd.svca = dte_evt.svca;
      }

      isbd_evt_t isbd_evt;

      isbd_evt.id = ISBD_EVT_DTE;
      isbd_evt.dte = dte_evt;

      k_msgq_put( ISBD_EVT_Q, &isbd_evt, K_NO_WAIT );
    }

  }

}

void isbd_destroy_mo_msg( struct isbd_mo_msg *mo_msg ) {

  if ( mo_msg->data ) {
    k_free( mo_msg->data );
  }

  mo_msg->len = 0;
  mo_msg->data = NULL;
}

void isbd_destroy_evt( isbd_evt_t *evt ) {

  switch ( evt->id ) {

    case ISBD_EVT_MO:
      isbd_destroy_mo_msg( &evt->mo );
      break;

    case ISBD_EVT_MT:
      isbd_destroy_mt_msg( &evt->mt );
      break;
  
    default:
      break;
  }

}

void _requeue_mo_msg( struct isbd_mo_msg *mo_msg ) {
  if ( k_msgq_put( ISBD_MO_Q, mo_msg, K_NO_WAIT ) == 0 ) {
    LOG_DBG( "MO message requeued" ); 
  }
}

/**
 * @brief Enqueues a message to be sent.
 * 
 * @param msg Message buffer
 * @param msg_len Message buffer length
 */
void isbd_enqueue_mo_msg( const uint8_t *msg, uint16_t msg_len, uint8_t retries ) {

  struct isbd_mo_msg mo_msg;
  
  // TODO: generate message id or let the user give an identifier
  mo_msg.retries = retries;

  if ( msg && msg_len > 0 ) {
    mo_msg.len = msg_len;
    mo_msg.data = (uint8_t*)k_malloc( sizeof(uint8_t) * msg_len );
    // TODO: check malloc
    memcpy( mo_msg.data, msg, msg_len );
  } else {
    mo_msg.len = 0;
    mo_msg.data = NULL;
  }

  if ( k_msgq_put( ISBD_MO_Q, &mo_msg, K_NO_WAIT ) == 0 ) {
    LOG_DBG( "MO message enqueued, len=%hu", msg_len );
  }
  
}

void isbd_request_mt_msg() {

  // if the queue already has pending session requests
  // there is no need to push a new one
  if ( k_msgq_num_used_get( ISBD_MO_Q ) == 0 ) {
    isbd_enqueue_mo_msg( NULL, 0, 0 );
  }

}

void isbd_setup( isbd_config_t *isbd_conf ) {

  g_isbd.svca = 0;
  g_isbd.cnf = *isbd_conf;

  g_isbd.mo_msgq_buf = 
    (char*) k_malloc( sizeof( struct isbd_mo_msg ) * g_isbd.cnf.mo_queue_len );

  // g_isbd.mt_msgq_buf = 
  //   (char*) k_malloc( sizeof( struct msgq_item ) * g_isbd.cnf.mt_queue_len );

  g_isbd.evt_msgq_buf = 
    (char*) k_malloc( sizeof( struct isbd_evt ) * g_isbd.cnf.evt_queue_len );

  k_msgq_init( 
    ISBD_MO_Q,
    g_isbd.mo_msgq_buf, 
    sizeof( struct isbd_mo_msg ), 
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

bool isbd_wait_evt( isbd_evt_t *isbd_evt, uint32_t timeout_ms ) {
  return k_msgq_get( ISBD_EVT_Q, isbd_evt, K_MSEC( timeout_ms ) ) == 0;
}

#define ISBD_ERR_CASE_RET_NAME( err ) \
  case err: \
    return #err;

const char* isbd_err_name( isbd_err_t err ) {

  switch ( err ) {
    
    ISBD_ERR_CASE_RET_NAME( ISBD_OK );
    ISBD_ERR_CASE_RET_NAME( ISBD_ERR_MO );
    ISBD_ERR_CASE_RET_NAME( ISBD_ERR_MT );
    ISBD_ERR_CASE_RET_NAME( ISBD_ERR_SPACE );
    
    default:
      return "ISBD_ERR_UNKNOWN";

  }
}