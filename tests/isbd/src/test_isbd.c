#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/multi_heap.h>

#include "isu.h"
#include "isbd.h"
#include "isu/dte.h"

#define RX_BUF_SIZE    256
#define TX_BUF_SIZE    256

#define ISBD_UART_NODE        DT_NODELABEL( uart1 )
#define ISBD_UART_DEVICE      DEVICE_DT_GET( ISBD_UART_NODE )

static uint8_t rx_buf[ RX_BUF_SIZE ];
static uint8_t tx_buf[ TX_BUF_SIZE ];

static isu_dte_config_t g_isu_dte_config = {
  .at_uart = {
    .echo = false,
    .verbose = false,
    // .zuart = ZUART_CONF_POLL( uart_slave_device ),
    .zuart = ZUART_CONF_IRQ( (struct device*)ISBD_UART_DEVICE, rx_buf, RX_BUF_SIZE, tx_buf, TX_BUF_SIZE ),
    // .zuart = ZUART_CONF_MIX_RX_IRQ_TX_POLL( uart_slave_device, rx_buf, sizeof( rx_buf ) ),
    // .zuart = ZUART_CONF_MIX_RX_POLL_TX_IRQ( uart_slave_device, tx_buf, sizeof( tx_buf ) ),
  }
};

static isu_dte_t g_isu_dte;

static void* isbd_suite_setup(void) {
  
  struct uart_config config;

  uart_config_get( ISBD_UART_DEVICE, &config );
  config.baudrate = 19200;
  uart_configure( ISBD_UART_DEVICE, &config );

  isu_dte_err_t ret;
  ret = isu_dte_setup( &g_isu_dte, &g_isu_dte_config );

  zassert_equal( ret, ISU_DTE_OK, "Setup failed" );

  return NULL;
}


ZTEST( isbd_suite, test_imei ) {

  int16_t ret;
  char imei[ 16 ];

  ret = isu_get_imei( &g_isu_dte, imei, sizeof( imei ) );
  
  zassert_equal( ret, ISU_DTE_OK, "Could not fetch IMEI" );
  zassert_equal( strlen( imei ), 15, "IMEI length mismatch" );
  
}

ZTEST( isbd_suite, test_mo_mt ) {

  /**
   * @brief Set mobile originated buffer with a
   * sample message
   */
  isu_dte_err_t ret;
  const uint8_t msg[] = { 
    0x34, 0x1E, 0x45, 0x23, 0x34, 0xBB, 0xCC, 0x54, 0x32, 0x13, 0x34, 0xA5
  };
  const uint16_t msg_len = sizeof( msg );
  const uint16_t msg_csum = isbd_compute_checksum( msg, msg_len );

  ret = isu_set_mo( &g_isu_dte, msg, sizeof( msg ) );
  
  zassert_equal( ret, ISU_DTE_OK, 
    "Could not set MO buffer" );

  /**
   * @brief Transfer mobile originated buffer to the
   * mobile terminated buffer
   */
  ret = isu_mo_to_mt( &g_isu_dte, NULL, 0 );
  
  // check if the MT buffer was successfully read
  zassert_equal( ret, ISU_DTE_OK, 
    "Could not transfer message from MO to MT" );

  /**
   * @brief Now we check if the mobile terminated buffer
   * contains exactly the same as the original message
  */
  uint16_t mt_csum;
  char mt_buf[ msg_len ];
  uint16_t mt_len = sizeof( mt_buf );

  ret = isu_get_mt( &g_isu_dte, mt_buf, &mt_len, &mt_csum );

  // check if MT buffer could be fetched
  zassert_equal( ret, ISU_DTE_OK, 
    "Could not fetch MT buffer" );

  zassert_equal( mt_len, msg_len, 
    "MT message length mismatch" );
  
  // check if message content is exactly the same
  for ( uint16_t i=0; i < mt_len; i++ ) {
    zassert_equal( mt_buf[ i ], msg[ i ], 
      "MT message content mismatch" );
  }

  // compute checksum
  zassert_equal( msg_csum, mt_csum, 
    "MT checksum mismatch" );

  /**
   * @brief Here we try to send an empty message
   * expecting an error result
   */
  const char empty_msg[] = {};
  ret = isu_set_mo( &g_isu_dte, empty_msg, sizeof( empty_msg ) );

  zassert_equal( ret, ISU_DTE_ERR_CMD );
  zassert_equal( isu_dte_get_err( &g_isu_dte ), 3 );

  // - 0 - SBD message successfully written to the 9602.
  // - 1 - SBD message write timeout. An insufficient number of bytes were transferred to 9602
  // during the transfer period of 60 seconds.
  // - 2 - SBD message checksum sent from DTE does not match the checksum calculated by the
  // 9602.
  // - 3 - SBD message size is not correct. The maximum mobile originated SBD message length
  // is 340 bytes. The minimum mobile originated SBD message length is 1 byte.

}

// TODO: move this to a new AT-UART test module
ZTEST( isbd_suite, test_overflow ) {

  int16_t ret;
  char imei[ 15 ];

  // this should trigger overflow error because
  // there is not enough space to write trailing \0
  ret = isu_get_imei( &g_isu_dte, imei, sizeof( imei ) );
  
  zassert_equal( ret, ISU_DTE_ERR_AT );
  zassert_equal( isu_dte_get_err( &g_isu_dte ), AT_UART_OVERFLOW, 
    "Overflow not triggered" );
  
}

ZTEST_SUITE( isbd_suite, NULL, isbd_suite_setup, NULL, NULL, NULL );
