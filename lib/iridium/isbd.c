
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "isu.h"
#include "isbd.h"
#include "isbd/evt.h"
#include "isbd/msg.h"

#define THREAD_STACK_SIZE   4096

#define ISBD_DTE \
  g_isbd.cnf.dte

#define ISBD_MO_Q \
  &g_isbd.mo_msgq

#define ISBD_MT_Q \
  &g_isbd.mt_msgq
  
typedef struct isbd {
  char *mo_msgq_buf;
  char *mt_msgq_buf;
  struct k_msgq mo_msgq;
  struct k_msgq mt_msgq;
  isbd_config_t cnf;
} isbd_t;

struct msgq_item {
  uint8_t *msg;
  uint8_t retries;
  uint16_t msg_len;
};

extern void _entry_point( void *, void *, void * );

void isbd_msg_destroy( struct msgq_item *msgq_item );
void isbd_msg_requeue( struct msgq_item *msgq_item );

static struct isbd g_isbd;
static struct k_thread g_thread_data;

K_THREAD_STACK_DEFINE( g_thread_stack_area, THREAD_STACK_SIZE );

bool _send( const uint8_t *msg, uint16_t msg_len ) {

  bool sent = false;
  printk( "Sending message #%hu ...\n", msg_len ); 
  
  isu_dte_err_t ret;

  if ( msg_len > 0 ) {
    ret = isu_set_mo( ISBD_DTE, msg, msg_len );
  } else {
    ret = isu_clear_buffer( ISBD_DTE, ISU_CLEAR_MO_BUFF );
  }

  if ( ret == ISU_DTE_OK ) {

    isu_session_ext_t session;
    ret = isu_init_session( ISBD_DTE, &session, false );

    if ( ret == ISU_DTE_OK ) {

      if ( session.mo_sts < 3 ) {
        sent = true;
      }

      if ( session.mt_sts == 1 ) {
        // TODO: read mobile terminated buffer 
        // TODO: and put it into the queue
      }

    }

  }
  
  return sent;

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

  while ( 1 ) {

    struct msgq_item msgq_item;

    if ( k_msgq_get( ISBD_MO_Q, &msgq_item, K_NO_WAIT ) == 0 ) {

      if ( _send( msgq_item.msg, msgq_item.msg_len ) ) {

        printk( "Message sent\n" );
        isbd_msg_destroy( &msgq_item );

      } else {

        printk( "Message not sent\n" );

        if ( msgq_item.retries > 0 ) {
          msgq_item.retries--;
          isbd_msg_requeue( &msgq_item );
        }

      }
    }

    isbd_evt_t evt;

    printk( "Waiting for event ...\n" );
    if ( isbd_evt_wait( ISBD_DTE, &evt, 1000 ) == ISU_DTE_OK ) {
      printk( "Event (%03d) captured\n", evt.id );
    }


  }

}

void isbd_msg_destroy( struct msgq_item *msgq_item ) {
  k_free( msgq_item->msg );
  msgq_item->msg = NULL;
  msgq_item->msg_len = 0;
  msgq_item->retries = 0;
}

void isbd_msg_requeue( struct msgq_item *msgq_item ) {
  if ( k_msgq_put( ISBD_MO_Q, msgq_item, K_NO_WAIT ) == 0 ) {
    printk( "Message #%hu was re-enqueued\n", 
      msgq_item->msg_len );
  }
}

/**
 * @brief Enqueues a message to be sent.
 * 
 * @param msg Message buffer
 * @param msg_len Message buffer length
 */
void isbd_msg_enqueue( const uint8_t *msg, uint16_t msg_len, uint8_t retries ) {

  struct msgq_item msgq_item;
  
  msgq_item.msg_len = msg_len;
  msgq_item.retries = retries;
  msgq_item.msg = (uint8_t*)k_malloc( sizeof(uint8_t) * msg_len );

  memcpy( msgq_item.msg, msg, msg_len );

  if ( k_msgq_put( ISBD_MO_Q, &msgq_item, K_NO_WAIT ) == 0 ) {
    printk( "Message #%hu was enqueued\n", msg_len );
  }

}

void isbd_setup( isbd_config_t *isbd_conf ) {

  g_isbd.cnf = *isbd_conf;

  g_isbd.mo_msgq_buf = 
    (char*) k_malloc( sizeof( struct msgq_item ) * g_isbd.cnf.mo_queue_len );

  g_isbd.mt_msgq_buf = 
    (char*) k_malloc( sizeof( struct msgq_item ) * g_isbd.cnf.mt_queue_len );

  k_msgq_init( 
    ISBD_MO_Q, 
    g_isbd.mo_msgq_buf, 
    sizeof( struct msgq_item ), 
    g_isbd.cnf.mo_queue_len );

  k_msgq_init( 
    ISBD_MT_Q, 
    g_isbd.mt_msgq_buf, 
    sizeof( struct msgq_item ), 
    g_isbd.cnf.mt_queue_len );

  // k_tid_t my_tid = k_thread_create( 
  k_thread_create( 
    &g_thread_data, g_thread_stack_area,
    K_THREAD_STACK_SIZEOF( g_thread_stack_area ),
    _entry_point,
    NULL, NULL, NULL,
    g_isbd.cnf.priority, 0, K_NO_WAIT );

}

void isbd_destroy() {
  // TODO
  return;
}
