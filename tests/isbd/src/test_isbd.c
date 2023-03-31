#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/multi_heap.h>

#include "isbd.h"

#define RX_BUF_SIZE    256
#define TX_BUF_SIZE    256

#define ISBD_UART_NODE        DT_NODELABEL(uart1)
#define ISBD_UART_DEVICE      DEVICE_DT_GET( ISBD_UART_NODE )

static uint8_t rx_buf[ RX_BUF_SIZE ];
static uint8_t tx_buf[ TX_BUF_SIZE ];

static struct isbd_config isbd_config = {
  .at_uart = {
    .echo = false,
    .verbose = false,
    // .zuart = ZUART_CONF_POLL( uart_slave_device ),
    .zuart = ZUART_CONF_IRQ( (struct device*)ISBD_UART_DEVICE, rx_buf, RX_BUF_SIZE, tx_buf, TX_BUF_SIZE ),
    // .zuart = ZUART_CONF_MIX_RX_IRQ_TX_POLL( uart_slave_device, rx_buf, sizeof( rx_buf ) ),
    // .zuart = ZUART_CONF_MIX_RX_POLL_TX_IRQ( uart_slave_device, tx_buf, sizeof( tx_buf ) ),
  }
};

static void* isbd_suite_setup(void) {

  // zuart_config_t zuart_config = ZUART_CONF_POLL( uart_slave_device );
  struct uart_config config;

  uart_config_get( ISBD_UART_DEVICE, &config );
  config.baudrate = 19200;
  uart_configure( ISBD_UART_DEVICE, &config );

  isbd_err_t ret;

  ret = isbd_setup( &isbd_config );

  zassert_equal( ret, ISBD_OK, "Could not talk to modem, probably busy ..." );

  return NULL;
}

ZTEST_SUITE( isbd_suite, NULL, isbd_suite_setup, NULL, NULL, NULL );

ZTEST( isbd_suite, test_imei ) {

  int16_t ret;
  char imei[ 16 ];

  ret = isbd_get_imei( imei, sizeof( imei ) );

  zassert_equal( ret, AT_UART_OK, "Could not fetch IMEI" );
  zassert_equal( strlen( imei ), 15, "IMEI length mismatch" );
  
}

ZTEST( isbd_suite, test_mo ) {

  int16_t ret;
  const char *msg = "HOLA";

  ret = isbd_set_mo( msg, strlen( msg ) );
  zassert_equal( ret, AT_UART_OK, "Could not set MO buffer => %d", ret );

}

// TODO: move this to a new AT-UART test module
ZTEST( isbd_suite, test_overflow ) {

  int16_t ret;
  char imei[ 15 ];

  // this should trigger overflow error because
  // there is not enough space to write trailing \0
  ret = isbd_get_imei( imei, sizeof( imei ) );

  zassert_equal( ret, AT_UART_OVERFLOW );  
}