/**
 * @file main.c
 * @author Javier Romera Llave (lromerajdev@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(isu_sbd_cmds, LOG_LEVEL_DBG );

#include "isu.h"
#include "isu/dte.h"
#include "isbd/util.h"

#include "shared.h"

#define MSG_SIZE 32

static isu_dte_t g_isu_dte;
static struct device *uart_960x_device = UART_960X_DEVICE;

static void isbd_print_error( isu_dte_t *dte, isu_dte_err_t err ) {

  if ( err == ISU_DTE_ERR_AT ) {
    printk( "(%03d) @ ISU_DTE_ERR_AT", isu_dte_get_err( dte ) );
  } else if ( err == ISU_DTE_ERR_CMD ) {
    printk( "(%03d) @ ISU_DTE_ERR_CMD", isu_dte_get_err( dte ) );
  } else {
    printk( "(\?\?\?) @ ISU_DTE_ERR_UNK" );
  }

}

#define TEST_ISU_CMD(ok_block, err_block, f_name, ... ) \
do { \
  printk( "%-20s() ", #f_name ); \
  int16_t M_g_code = f_name( &g_isu_dte, __VA_ARGS__ ); \
  if ( M_g_code == ISU_DTE_OK ) { \
    printk( "OK; " ); \
    ok_block \
  } else { \
    printk( "ERR: " ); isbd_print_error( &g_isu_dte, M_g_code ); printk( ";" ); \
    err_block \
  } \
  printk( "\n" ); \
} while(0)

static uint8_t rx_buf[ 512 ];
static uint8_t tx_buf[ 512 ];

int main(void) {

  // if ( !gpio_is_ready_dt( &red_led )
  //     || !gpio_is_ready_dt( &blue_led )
  //     || !gpio_is_ready_dt( &green_led ) ) {
    
  //   printk( "LED device not found\n" );
  //   return;
  // }

	if ( !device_is_ready( uart_960x_device ) ) {
		printk( "UART device not found\n" );
		return 1;
  }

  struct uart_config uart_config;

	uart_config_get( uart_960x_device, &uart_config );
	uart_config.baudrate = 19200;
	uart_configure( uart_960x_device, &uart_config );

  isu_dte_config_t isu_dte_config = {
    .at_uart = {
      .echo = true,
      .verbose = true,
      // .zuart = ZUART_CONF_POLL( 960x_device ),
      .zuart = ZUART_CONF_IRQ( uart_960x_device, rx_buf, sizeof( rx_buf ), tx_buf, sizeof( tx_buf ) ),
      // .zuart = ZUART_CONF_MIX_RX_IRQ_TX_POLL( 960x_device, rx_buf, sizeof( rx_buf ) ),
      // .zuart = ZUART_CONF_MIX_RX_POLL_TX_IRQ( 960x_device, tx_buf, sizeof( tx_buf ) ),
    }
  };

  char buf[ 256 ]; // buffer used to parse AT responses

  if ( isu_dte_setup( &g_isu_dte, &isu_dte_config ) == ISU_DTE_OK ) {
    printk( "Modem OK\n" );
  } else {
    printk( "Could not talk to modem, probably busy ...\n" );
    return 1;
  }

  TEST_ISU_CMD({
    printk( "Revision: %s", buf );
  }, {}, isu_get_revision, buf, sizeof( buf ) );

  // enable alerts
  TEST_ISU_CMD({
    printk( "MT Alert successfully enabled" );
  }, {}, isu_set_mt_alert, ISU_MT_ALERT_ENABLED );

  isu_mt_alert_t alert;
  TEST_ISU_CMD({
    printk( "MT Alert: %d", alert );
  }, {}, isu_get_mt_alert, &alert );  

  TEST_ISU_CMD({
    printk( "IMEI: %s", buf );
  }, {}, isu_get_imei, buf, sizeof( buf ) );  

  const char *msg = "MIoT";

  TEST_ISU_CMD({}, {}, isu_set_mo, msg, strlen( msg ) );

  TEST_ISU_CMD({
    printk( "%s", buf );
  }, {}, isu_mo_to_mt,  buf, sizeof( buf ) );

  uint16_t csum;
  uint16_t len = sizeof( buf );
  
  TEST_ISU_CMD({
    printk("msg=\"");
    for ( int i=0; i < len; i++ ) {
      printk( "%c", buf[ i ] );
    }
    printk( "\", len=%d, csum=%04X == %04X", 
      len, csum, isbd_util_compute_checksum( buf, len ) );
  }, {}, isu_get_mt, buf, &len, &csum );


  isu_ring_sts_t ring_sts;
  TEST_ISU_CMD({
    printk( "Ring status: %d", ring_sts );
  }, {}, isu_get_ring_sts, &ring_sts );

  isu_session_ext_t session;

  TEST_ISU_CMD({ // success

    printk( "mo_sts=%hhu, "
            "mo_msn=%hu, "
            "mt_sts=%hhu, "
            "mt_msn=%hu, "
            "mt_length=%hu, "
            "mt_queued=%hu",
    session.mo_sts,
    session.mo_msn,
    session.mt_sts,
    session.mt_msn,
    session.mt_len,
    session.mt_queued );

  }, { }, isu_init_session, &session, false );

  TEST_ISU_CMD({
    printk( "Mobile buffers cleared" );
  }, {}, isu_clear_buffer, ISU_CLEAR_MO_MT_BUFF );

  /*
  isu_evt_report_t evt_report = {
    .mode = 1,
    .service = 1,
    .signal = 1,
  };

  TEST_ISU_CMD( {}, {}, isu_set_evt_report, &evt_report );
  */

  /*
  while (1) {

    // TODO: this logic should be moved to isbd module, 
    // TODO: this is only for testing purposes
    // Remember that this blocking call will be interrupted if any command
    // is externally executed
    if ( AT_UART_UNK == at_uart_pack_txt_resp( buf, sizeof( buf ), AT_1_LINE_RESP, 10000 ) ) {
      printk("RECEIVED: %s\n", buf );
    }

  }
  */

  /*
  uint8_t signal;
  TEST_ISU_CMD({
    printk( "signal_quality=%d", signal );
  }, isu_get_sig_q, &signal );
  */

  // code = isbd_set_mo_bin( msg, strlen( msg ) );

  // code = isu_clear_buffer( ISU_CLEAR_MO_BUFF );
  // printk( "isu_clear_buffer() : %d\n", code );

  /*
  uint16_t len, csum;
  isbd_get_mt_bin( __buff, &len, &csum );
  
  for ( int i=0; i < len; i++ ) {
    printk( "%c", __buff[ i ] );
  }

  printk( " @ len = %d, csum = %04X\n", len, csum );
  */
  


  // int8_t cmd_code = isu_set_mo_txt( "hoooooo" );
  // printk( "SBDWT    : %d\n", cmd_code );

  // isu_mo_to_mt( __buff );
  // printk( "MO -> MT : %s\n", __buff );

  // isu_get_mt_txt( __buff );
  // printk( "MT       : %s\n", __buff );
  
  /*
  uint16_t len, csum;
  isbd_get_mt_bin( __buff, &len, &csum );
  
  for ( int i=0; i < len; i++ ) {
    printk( "%c", __buff[ i ] );
  }
  printk( " @ len = %d, csum = %04X\n", len, csum );
  */


  /*
  isu_set_mo_txt(
    "This is an example message. It has more than 64 bytes or I think so, we should type a little bit more abcdefghijklmnopqW" );

  isu_mo_to_mt( __buff );
  printk( "MO -> MT : %s\n", __buff );

  isu_get_mt_txt( __buff );
  printk( "MT       : %s\n", __buff );
 */

  // isbd_fetch_revision( revision );
  // printk( "Revision  : %s\n", revision );

  return 0;
} 
