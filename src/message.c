
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "isbd.h"

#include "message.h"

#define QUEUE_MAX_LEN       10

#define THREAD_PRIORITY     0
#define THREAD_STACK_SIZE   4096

/* change this to any other UART peripheral if desired */
// #define UART_MASTER_DEVICE_NODE DT_NODELABEL(uart0)
#define UART_SLAVE_DEVICE_NODE DT_NODELABEL( uart3 )

static struct device *uart_slave_device = 
  (struct device*)DEVICE_DT_GET( UART_SLAVE_DEVICE_NODE );

static struct k_thread g_thread_data;
K_THREAD_STACK_DEFINE( g_thread_stack_area, THREAD_STACK_SIZE);

extern void _entry_point( void *, void *, void * );

struct msgq_item {
  uint8_t *msg;
  uint16_t msg_len;
};

static struct k_msgq g_msgq;

static char g_msgq_buf[ QUEUE_MAX_LEN * sizeof(struct msgq_item) ];

static uint8_t rx_buf[ 512 ];
static uint8_t tx_buf[ 512 ];


void msg_destroy( struct msgq_item *msgq_item );
void msg_requeue( struct msgq_item *msgq_item );

bool _send( const uint8_t *msg, uint16_t msg_len ) {

  printk( "Sending message #%hu ...\n", msg_len ); 
  
  isbd_err_t ret;
  ret = isbd_set_mo( msg, msg_len );

  if ( ret == ISBD_OK ) {

    isbd_session_ext_t session;
    ret = isbd_init_session( &session, false );

    if ( ret == ISBD_OK && session.mo_sts < 3 ) {
      return true;
    }

  }
  
  return false;

}

void _entry_point( void *v1, void *v2, void *v3 ) {

  struct uart_config uart_config;

	uart_config_get( uart_slave_device, &uart_config );
	uart_config.baudrate = 19200;
	uart_configure( uart_slave_device, &uart_config );

  struct isbd_config isbd_config = {
    .at_uart = {
      .echo = true,
      .verbose = true,
      // .zuart = ZUART_CONF_POLL( uart_slave_device ),
      .zuart = ZUART_CONF_IRQ( uart_slave_device, rx_buf, sizeof( rx_buf ), tx_buf, sizeof( tx_buf ) ),
      // .zuart = ZUART_CONF_MIX_RX_IRQ_TX_POLL( uart_slave_device, rx_buf, sizeof( rx_buf ) ),
      // .zuart = ZUART_CONF_MIX_RX_POLL_TX_IRQ( uart_slave_device, tx_buf, sizeof( tx_buf ) ),
    }
  };

  if ( isbd_setup( &isbd_config ) == ISBD_OK ) {
    printk( "Modem OK\n" );
  } else {
    printk( "Could not talk to modem, probably busy ...\n" );
    return;
  }

  isbd_evt_report_t evt_report = {
    .mode = 1,
    .service = 1,
    .signal = 1,
  };

  isbd_set_evt_report( &evt_report );

  while ( 1 ) {

    struct msgq_item msgq_item;

    if ( k_msgq_get( &g_msgq, &msgq_item, K_NO_WAIT ) == 0 ) {
      if ( _send( msgq_item.msg, msgq_item.msg_len ) ) {
        printk( "Message sent\n" );
        msg_destroy( &msgq_item );
      } else {
        printk( "Message not sent\n" );
        msg_requeue( &msgq_item );
      }
    }

    isbd_event_t evt;

    // printk( "Waiting for event ...\n" );
    if ( isbd_wait_event( &evt, 1000 ) == ISBD_OK ) {
      printk( "Event (%03d) captured\n", evt.name );
    }

  }

}

void msg_destroy( struct msgq_item *msgq_item ) {
  free( msgq_item->msg );
  msgq_item->msg = NULL;
  msgq_item->msg_len = 0;
}

void msg_requeue( struct msgq_item *msgq_item ) {
  if ( k_msgq_put( &g_msgq, msgq_item, K_NO_WAIT ) == 0 ) {
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
void msg_enqueue( const uint8_t *msg, uint16_t msg_len ) {

  struct msgq_item msgq_item;
  
  msgq_item.msg_len = msg_len;
  msgq_item.msg = (uint8_t*)k_malloc( sizeof(uint8_t) * msg_len );

  memcpy( msgq_item.msg, msg, msg_len );

  if ( k_msgq_put( &g_msgq, &msgq_item, K_NO_WAIT ) == 0 ) {
    printk( "Message #%hu was enqueued\n", msg_len );
  }

}

void msg_setup() {

  k_msgq_init( &g_msgq, g_msgq_buf, sizeof(struct msgq_item), QUEUE_MAX_LEN );

  k_tid_t my_tid = k_thread_create( 
    &g_thread_data, g_thread_stack_area,
    K_THREAD_STACK_SIZEOF( g_thread_stack_area ),
    _entry_point,
    NULL, NULL, NULL,
    THREAD_PRIORITY, 0, K_NO_WAIT );

}
