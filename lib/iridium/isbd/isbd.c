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

#define DTE_EVT_WAIT_TIMEOUT    CONFIG_ISBD_DTE_EVT_WAIT_TIMEOUT

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
  uint8_t sigq;
  char *mo_msgq_buf;
  char *mt_msgq_buf;
  char *evt_msgq_buf;
  struct k_msgq mo_msgq;
  struct k_msgq mt_msgq;
  struct k_msgq evt_msgq;
  isbd_config_t cnf;
} isbd_t;

extern void _entry_point( void *, void *, void * );
static void _wait_for_dte_events( uint32_t timeout_ms );
isbd_err_t _enqueue_mo_msg( struct isbd_mo_msg *mo_msg );

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
        _enqueue_mo_msg( mo_msg );
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

        if ( session->mt_queued > 0 ) {
          isbd_request_session( false );
        }

      } else {
        _notify_err( ISBD_ERR_MT );
        isbd_destroy_mt_msg( &mt_msg );
      }

    } else {
      LOG_ERR( "%s", "Could not alloc memory for MT message" );
      // TODO: the message still remains in the ISU memory, try again later ?
    }

  }

}

void _init_session( struct isbd_mo_msg *mo_msg ) {

  isu_dte_err_t ret;

  if ( mo_msg->data && mo_msg->len > 0 ) {
    
    LOG_DBG( "Setting MO buffer, len=%hu", mo_msg->len );
    ret = isu_set_mo( ISBD_DTE, mo_msg->data, mo_msg->len );

    if ( ret != ISU_DTE_OK ) {
      isbd_destroy_mo_msg( mo_msg );
      LOG_ERR( "%s", "Could not set MO buffer" );
    }

  } else {
    LOG_DBG( "Clearing MO buffer" );
    ret = isu_clear_buffer( ISBD_DTE, ISU_CLEAR_MO_BUFF );
  }

  if ( ret == ISU_DTE_OK ) {

    isu_session_ext_t session;
    ret = isu_init_session( ISBD_DTE, &session, mo_msg->alert );

    // Fixes: https://glab.lromeraj.net/ucm/miot/tfm/iridium-sbd-library/-/issues/27
    _wait_for_dte_events( 10 );

    if ( ret == ISU_DTE_OK ) {
      _handle_session_mo_msg( &session, mo_msg );
      _handle_session_mt_msg( &session );
    } else {
      LOG_ERR( "Could not init session %d\n", ret );
    }

  }

}

isbd_err_t isbd_destroy_mt_msg( struct isbd_mt_msg *mt_msg ) {

  if ( mt_msg->data ) {
    k_free( mt_msg->data );
  }
  mt_msg->len = 0;

  return ISBD_OK;
}

void _entry_point( void *v1, void *v2, void *v3 ) {

  isu_evt_report_t evt_report = {
    .mode     = 1,
    .signal   = 1,
    .service  = 1,
  };

  isu_dte_err_t dte_err;
  

  dte_err = isu_set_evt_report(
    ISBD_DTE, &evt_report, &g_isbd.sigq, &g_isbd.svca );

  if ( dte_err == ISU_DTE_OK ) {
    LOG_DBG( "svca=%hhu, sigq=%hhu", g_isbd.svca, g_isbd.sigq );
  } else {
    LOG_ERR( "%s", "Could not set event reporting" );
  }

  dte_err = isu_set_mt_alert( ISBD_DTE, ISU_MT_ALERT_ENABLED );

  if ( dte_err == ISU_DTE_OK ) {
    LOG_INF( "%s", "Ring alerts enabled" );
  } else {
    LOG_ERR( "%s", "Could not enable ring alerts" );
  }

  DO_FOREVER {

    struct isbd_mo_msg mo_msg;

    // sessions will be sent only if the service is currently available
    if ( g_isbd.svca && g_isbd.sigq >= g_isbd.cnf.sigq_threshold ) {
      if ( k_msgq_get( ISBD_MO_Q, &mo_msg, K_NO_WAIT ) == 0 ) {
        _init_session( &mo_msg );
      }
    }

    _wait_for_dte_events( DTE_EVT_WAIT_TIMEOUT );
  }

}

static void _wait_for_dte_events( uint32_t timeout_ms ) {

  isu_dte_err_t dte_err;
  isu_dte_evt_t dte_evt;

  dte_err = isu_dte_evt_wait(
    ISBD_DTE, &dte_evt, timeout_ms );

  if ( dte_err == ISU_DTE_OK ) {

    isbd_evt_t isbd_evt = {
      .id = ISBD_EVT_UNK,
    };

    if ( dte_evt.id == ISU_DTE_EVT_SVCA ) {
      g_isbd.svca = dte_evt.svca;
      isbd_evt.id = ISBD_EVT_SVCA;
      isbd_evt.svca = dte_evt.svca;
    } else if ( dte_evt.id == ISU_DTE_EVT_SIGQ ) {
      g_isbd.sigq = dte_evt.sigq;
      isbd_evt.id = ISBD_EVT_SIGQ;
      isbd_evt.sigq = dte_evt.sigq;
    } else if ( dte_evt.id == ISU_DTE_EVT_RING ) {
      isbd_evt.id = ISBD_EVT_RING;
    }

    k_msgq_put( ISBD_EVT_Q, &isbd_evt, K_NO_WAIT );

    // ! If there is more than one event in the reception buffer
    // ! we have to wait a little for it to avoid a possible event loss
    _wait_for_dte_events( 10 );

  }


} 

isbd_err_t isbd_destroy_mo_msg( struct isbd_mo_msg *mo_msg ) {

  if ( mo_msg->data ) {
    k_free( mo_msg->data );
  }

  mo_msg->len = 0;
  mo_msg->data = NULL;

  return ISBD_OK;
}

isbd_err_t isbd_destroy_evt( isbd_evt_t *evt ) {

  switch ( evt->id ) {

    case ISBD_EVT_MO:
      return isbd_destroy_mo_msg( &evt->mo );

    case ISBD_EVT_MT:
      return isbd_destroy_mt_msg( &evt->mt );
  
    default:
      break;
  }

  return ISBD_OK;
}

/**
 * @brief Enqueues an MO message
 * 
 * @param msg Message buffer
 * @param msg_len Message buffer length
 */
isbd_err_t _enqueue_mo_msg( struct isbd_mo_msg *mo_msg ) {
  if ( k_msgq_put( ISBD_MO_Q, mo_msg, K_NO_WAIT ) == 0 ) {
    LOG_DBG( "MO message enqueued, len=%hu", mo_msg->len );
    return ISBD_OK; 
  }
  return ISBD_ERR_SPACE;
}

isbd_err_t isbd_send_mo_msg( 
  const uint8_t *msg, uint16_t msg_len, uint8_t retries 
) {

  struct isbd_mo_msg mo_msg;
  
  mo_msg.len = msg_len;
  mo_msg.alert = false;
  mo_msg.retries = retries;

  mo_msg.data = (uint8_t*)k_malloc( sizeof(uint8_t) * msg_len );
  
  if ( mo_msg.data == NULL ) {
    return ISBD_ERR_MEM;
  } else {
    memcpy( mo_msg.data, msg, msg_len );
  }

  // TODO: instead of doing this we could use a global flag
  // TODO: but we'll need extra synchronization mechanism 
  if ( k_msgq_num_used_get( ISBD_MO_Q ) == 1 ) {

    struct isbd_mo_msg _mo_msg;
    if ( k_msgq_get( ISBD_MO_Q, &_mo_msg, K_NO_WAIT ) == 0 ) {

      if ( _mo_msg.data == NULL ) { 
        // empty payload, so it's a simple session request
        
        // copy alert flag from the queued message to the current message
        mo_msg.alert = _mo_msg.alert;

        // ar there is no payload this is not mandatory, but recommended
        isbd_destroy_mo_msg( &_mo_msg );

      } else {
        // put message back again in the queue
        _enqueue_mo_msg( &_mo_msg );
      }

    }

  }

  return _enqueue_mo_msg( &mo_msg );
}

isbd_err_t isbd_request_session( bool alert ) {

  struct isbd_mo_msg mo_msg;
  
  mo_msg.len = 0;
  mo_msg.data = NULL;
  mo_msg.alert = alert;

  // if the queue already has pending session requests
  // there is no need to push a new one
  if ( k_msgq_num_used_get( ISBD_MO_Q ) == 0 ) {
    return _enqueue_mo_msg( &mo_msg );
  }

  return ISBD_OK;
}

isbd_err_t isbd_setup( isbd_config_t *isbd_conf ) {
  
  g_isbd.cnf      = *isbd_conf;
  g_isbd.svca     = 0;

  g_isbd.mo_msgq_buf = 
    (char*) k_malloc( sizeof( struct isbd_mo_msg ) * g_isbd.cnf.mo_queue_len );

  g_isbd.evt_msgq_buf = 
    (char*) k_malloc( sizeof( struct isbd_evt ) * g_isbd.cnf.evt_queue_len );

  if ( g_isbd.mo_msgq_buf == NULL ) {
    return ISBD_ERR_MEM;
  }

  if ( g_isbd.evt_msgq_buf == NULL ) {
    k_free( g_isbd.mo_msgq_buf );
    return ISBD_ERR_MEM;
  }

  k_msgq_init( 
    ISBD_MO_Q,
    g_isbd.mo_msgq_buf, 
    sizeof( struct isbd_mo_msg ), 
    g_isbd.cnf.mo_queue_len );

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

  return ISBD_OK;
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