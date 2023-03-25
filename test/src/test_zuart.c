#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/multi_heap.h>

#include "../../src/zuart.h"

#define UART_SLAVE_DEVICE_NODE DT_NODELABEL(uart1)

static const struct device *uart_slave_device = 
  DEVICE_DT_GET(UART_SLAVE_DEVICE_NODE);

struct zuart_suite_fixture {
  zuart_t zuart;
  uint8_t rx_buf[64];
  uint8_t tx_buf[64];
};

static void* zuart_suite_setup(void) {

  struct zuart_suite_fixture *fixture = 
    k_malloc( sizeof( struct zuart_suite_fixture ) );

  zassume_not_null( fixture );

  // zuart_config_t zuart_config = ZUART_CONF_POLL( uart_slave_device );
  zuart_config_t zuart_config = ZUART_CONF_IRQ( uart_slave_device, 
    fixture->rx_buf, sizeof( fixture->rx_buf ), fixture->tx_buf, sizeof( fixture->tx_buf ) );

  struct uart_config config;

  uart_config_get( uart_slave_device, &config );
  config.baudrate = 115200;
  uart_configure( uart_slave_device, &config );

  zassert_equal( zuart_setup( &fixture->zuart, &zuart_config ), ZUART_OK, "Setup error" );

  return fixture;
}

ZTEST_SUITE(zuart_suite, NULL, zuart_suite_setup, NULL, NULL, NULL);

uint32_t read_line( zuart_t *zuart ) {

  int32_t ret;
  uint8_t byte;
  uint32_t rx_size = 0;
  while ( (ret = zuart_read( zuart, &byte, 1, 0 ) ) >= 0 ) {
    rx_size += ret;
    if ( byte == '\n' ) break;
  }
  return rx_size;
}

ZTEST_F( zuart_suite, test_overrun ) {

  int32_t ret; 
  uint16_t write_buf_len;

  char read_buf[ 16 ];
  char write_buf[ 16 ];

  write_buf_len = sprintf( write_buf, "ping\n" );
  
  do {
    zuart_write( &fixture->zuart, write_buf, write_buf_len, 0 );
    
    // accept any byte as response
    ret = zuart_read( &fixture->zuart, read_buf, 1, 100 );

  } while ( ret != 1 );

  write_buf_len = sprintf( write_buf, "flood %d\n", sizeof( fixture->rx_buf ) + 1 );

  zuart_write( &fixture->zuart, write_buf, write_buf_len, 1000 );

  // wait enough to receive whole flood
  k_msleep( 100 );

  // overrun should be returned
  ret = zuart_read( &fixture->zuart, read_buf, 1, 0 );

  zassert_equal( ret, ZUART_ERR_OVERRUN, "Overrun not triggered" );

  write_buf_len = sprintf( write_buf, "close\n" );

  zuart_write( &fixture->zuart, write_buf, write_buf_len, 1000 );

  // printk( "RX SIZE: %d ~ %d\n", rx_size, (rx_size * 8 * 1000)/19200 );
  // printk( "TX SIZE: %d ~ %d\n", tx_size, (tx_size * 8 * 1000)/19200 );

	// zassert_false(0, "0 was true");
	// zassert_is_null(NULL, "NULL was not NULL");
	// zassert_not_null("foo", "\"foo\" was NULL");
	// zassert_equal_ptr(NULL, NULL, "NULL was not equal to NULL");
}
