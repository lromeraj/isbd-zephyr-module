/*
 * Copyright (c) 2022 Libre Solar Technologies GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "at.h"
#include "isbd.h"

#define MSG_SIZE 32

#define STACK_SIZE		2048
#define PRIORITY		  5

/* change this to any other UART peripheral if desired */
// #define UART_MASTER_DEVICE_NODE DT_NODELABEL(uart0)
#define UART_SLAVE_DEVICE_NODE DT_NODELABEL(uart3)

static const struct device *const uart_slave_device = DEVICE_DT_GET(UART_SLAVE_DEVICE_NODE);

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)

static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET( LED2_NODE, gpios );
static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET( LED1_NODE, gpios );
static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET( LED0_NODE, gpios );

#define TEST_AT_CMD(ok_block, err_block, f_name, ... ) \
do { \
  printk( "%-20s() ", #f_name ); \
  uint8_t M_g_code = f_name( __VA_ARGS__ ); \
  if ( M_g_code == 0 ) { \
    printk( "OK; " ); \
    ok_block \
    printk( "\n" ); \
  } else { \
    printk( "ERR: %s\n", at_uart_err_to_name( M_g_code ) ); \
    err_block \
  } \
} while(0)


void clear_leds() {
  gpio_pin_configure_dt( &blue_led, GPIO_OUTPUT_INACTIVE );
  gpio_pin_configure_dt( &red_led, GPIO_OUTPUT_INACTIVE );
  gpio_pin_configure_dt( &green_led, GPIO_OUTPUT_INACTIVE );
}

void set_warning_led() {
  clear_leds();

  gpio_pin_configure_dt( &red_led, GPIO_OUTPUT_ACTIVE );
  gpio_pin_configure_dt( &green_led, GPIO_OUTPUT_ACTIVE );
}

void set_success_led() {
  clear_leds();
  gpio_pin_configure_dt( &green_led, GPIO_OUTPUT_ACTIVE );
}

void set_info_led() {
  clear_leds();
  gpio_pin_configure_dt( &blue_led, GPIO_OUTPUT_ACTIVE );
}

void main(void) {

  /*
  if ( !gpio_is_ready_dt(&led) ) {
    return;
  }
  */

  gpio_pin_configure_dt( &blue_led, GPIO_OUTPUT_ACTIVE );

	if (!device_is_ready(uart_slave_device)) {
		printk("UART slave device not found!");
		return;
  }

	// print_uart( uart_master_device, "Hello! I'm your echo bot for Iridium 9602 module.\r\n\r\n" );

  struct uart_config uart_config;

	uart_config_get( uart_slave_device, &uart_config );
	uart_config.baudrate = 19200;
	uart_configure( uart_slave_device, &uart_config );

  struct isbd_config isbd_config = {
    .at_uart = {
      .echo = true,
      .verbose = true,
      .dev = uart_slave_device,
    }
  };

  char __buff[ 256 ];

  if ( isbd_setup( &isbd_config ) == ISBD_OK ) {
    printk( "Modem OK\n" );
  } else {
    printk( "Could not talk to modem, probably busy ...\n" );
    set_warning_led();
    return;
  }

  set_info_led();

  // isbd_fetch_imei( __buff, sizeof( __buff ) );
  // CHECK_AT_CMD( code, {}, isbd_enable_echo, true );

  TEST_AT_CMD({
    printk( "Revision : %s", __buff );
  }, {}, isbd_get_revision, __buff, sizeof( __buff ) );  

  TEST_AT_CMD({
    printk( "IMEI : %s", __buff );
  }, {}, isbd_get_imei, __buff, sizeof( __buff ) );  

  const char *msg = "hello";

  TEST_AT_CMD({}, {}, isbd_set_mo, msg, strlen( msg ) );

  TEST_AT_CMD({
    printk( "%s", __buff );
  }, {}, isbd_mo_to_mt, __buff, sizeof( __buff ) );

  

  uint16_t csum;
  size_t len = sizeof( __buff );

  TEST_AT_CMD({
    printk("msg=");
    for ( int i=0; i < len; i++ ) {
      printk( "%c", __buff[ i ] );
    }
    printk( ", len=%d, csum=%04X", len, csum );
  }, {}, isbd_get_mt, __buff, &len, &csum );

  /*
  uint8_t signal;
  TEST_AT_CMD({
    printk( "signal_quality=%d", signal );
  }, isbd_get_sig_q, &signal );
  */

  // code = isbd_set_mo_bin( msg, strlen( msg ) );

  // code = isbd_clear_buffer( ISBD_CLEAR_MO_BUFF );
  // printk( "isbd_clear_buffer() : %d\n", code );

  /*
  uint16_t len, csum;
  isbd_get_mt_bin( __buff, &len, &csum );
  
  for ( int i=0; i < len; i++ ) {
    printk( "%c", __buff[ i ] );
  }

  printk( " @ len = %d, csum = %04X\n", len, csum );
  */
  
  isbd_session_t session;


  TEST_AT_CMD({ // success

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

    if ( session.mo_sts < 3 ) {
      gpio_pin_configure_dt( &green_led, GPIO_OUTPUT_ACTIVE );
    } else {
      gpio_pin_configure_dt( &red_led, GPIO_OUTPUT_ACTIVE );
    }

  }, { // AT command failed
    gpio_pin_configure_dt( &red_led, GPIO_OUTPUT_ACTIVE );
  }, isbd_init_session, &session );

  gpio_pin_configure_dt( &blue_led, GPIO_OUTPUT_INACTIVE );

  // int8_t cmd_code = isbd_set_mo_txt( "hoooooo" );
  // printk( "SBDWT    : %d\n", cmd_code );

  // isbd_mo_to_mt( __buff );
  // printk( "MO -> MT : %s\n", __buff );

  // isbd_get_mt_txt( __buff );
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
  isbd_set_mo_txt(
    "This is an example message. It has more than 64 bytes or I think so, we should type a little bit more abcdefghijklmnopqW" );

  isbd_mo_to_mt( __buff );
  printk( "MO -> MT : %s\n", __buff );

  isbd_get_mt_txt( __buff );
  printk( "MT       : %s\n", __buff );
 */

  // isbd_fetch_revision( revision );
  // printk( "Revision  : %s\n", revision );

}
