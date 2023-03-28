#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/multi_heap.h>

#include "isbd.h"

#define UART_HOST_NODE DT_NODELABEL(uart1)

static const struct device *uart_slave_device = 
  DEVICE_DT_GET( UART_HOST_NODE );

#define RX_TX_BUF_SIZE    512

struct isbd_suite_fixture {
  uint8_t rx_buf[ RX_TX_BUF_SIZE ];
  uint8_t tx_buf[ RX_TX_BUF_SIZE ];
};

static void* isbd_suite_setup(void) {

  struct isbd_suite_fixture *fixture = 
    k_malloc( sizeof( struct isbd_suite_fixture ) );

  zassume_not_null( fixture );

  // zuart_config_t zuart_config = ZUART_CONF_POLL( uart_slave_device );
  struct uart_config config;

  uart_config_get( uart_slave_device, &config );
  config.baudrate = 19200;
  uart_configure( uart_slave_device, &config );

  struct isbd_config isbd_config = {
    .at_uart = {
      .echo = false,
      .verbose = true,
      // .zuart = ZUART_CONF_POLL( uart_slave_device ),
      .zuart = ZUART_CONF_IRQ( 
          uart_slave_device, 
          fixture->rx_buf, RX_TX_BUF_SIZE, 
          fixture->tx_buf, RX_TX_BUF_SIZE 
      ),
      // .zuart = ZUART_CONF_MIX_RX_IRQ_TX_POLL( uart_slave_device, rx_buf, sizeof( rx_buf ) ),
      // .zuart = ZUART_CONF_MIX_RX_POLL_TX_IRQ( uart_slave_device, tx_buf, sizeof( tx_buf ) ),
    }
  };

  if ( isbd_setup( &isbd_config ) == ISBD_OK ) {
    printk( "Modem OK\n" );
  } else {
    printk( "Could not talk to modem, probably busy ...\n" );
  }

  return fixture;
}

ZTEST_SUITE( isbd_suite, NULL, isbd_suite_setup, NULL, NULL, NULL );