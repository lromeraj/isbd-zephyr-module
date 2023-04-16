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

// LOG_MODULE_REGISTER( isu_sbd_cmds, LOG_LEVEL_DBG );

#include "isbd.h"

#define MSG_SIZE 32

/* change this to any other UART peripheral if desired */
// #define UART_MASTER_DEVICE_NODE DT_NODELABEL(uart0)
#define UART_SLAVE_DEVICE_NODE DT_NODELABEL( uart3 )

static struct device *uart_slave_device = 
  (struct device*)DEVICE_DT_GET( UART_SLAVE_DEVICE_NODE );

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS( led0 )
#define LED1_NODE DT_ALIAS( led1 )
#define LED2_NODE DT_ALIAS( led2 )

static const struct gpio_dt_spec red_led    = GPIO_DT_SPEC_GET( LED2_NODE, gpios );
static const struct gpio_dt_spec blue_led   = GPIO_DT_SPEC_GET( LED1_NODE, gpios );
static const struct gpio_dt_spec green_led  = GPIO_DT_SPEC_GET( LED0_NODE, gpios );

static isu_dte_t g_isu_dte;

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

void set_error_led() {
  clear_leds();
  gpio_pin_configure_dt( &red_led, GPIO_OUTPUT_ACTIVE );
}

void set_success_led() {
  clear_leds();
  gpio_pin_configure_dt( &green_led, GPIO_OUTPUT_ACTIVE );
}

void set_info_led() {
  clear_leds();
  gpio_pin_configure_dt( &blue_led, GPIO_OUTPUT_ACTIVE );
}

static uint8_t rx_buf[ 512 ];
static uint8_t tx_buf[ 512 ];

void main(void) {

  if ( !gpio_is_ready_dt( &red_led )
      || !gpio_is_ready_dt( &blue_led )
      || !gpio_is_ready_dt( &green_led ) ) {
    
    printk( "LED device not found\n" );
    return;
  }

	if ( !device_is_ready( uart_slave_device ) ) {
		printk( "UART device not found\n" );
		return;
  }

  struct uart_config uart_config;

	uart_config_get( uart_slave_device, &uart_config );
	uart_config.baudrate = 19200;
	uart_configure( uart_slave_device, &uart_config );

  isu_dte_config_t isu_dte_config = {
    .at_uart = {
      .echo = true,
      .verbose = true,
      // .zuart = ZUART_CONF_POLL( uart_slave_device ),
      .zuart = ZUART_CONF_IRQ( uart_slave_device, rx_buf, sizeof( rx_buf ), tx_buf, sizeof( tx_buf ) ),
      // .zuart = ZUART_CONF_MIX_RX_IRQ_TX_POLL( uart_slave_device, rx_buf, sizeof( rx_buf ) ),
      // .zuart = ZUART_CONF_MIX_RX_POLL_TX_IRQ( uart_slave_device, tx_buf, sizeof( tx_buf ) ),
    }
  };

  if ( isu_dte_setup( &g_isu_dte, &isu_dte_config ) == ISU_DTE_OK ) {
    printk( "Modem OK\n" );
    set_info_led();
  } else {
    printk( "Could not talk to modem, probably busy ...\n" );
    set_warning_led();
    return;
  }


  isbd_config_t isbd_config = {
    .dte            = &g_isu_dte,
    .priority       = 0, 
    .mo_queue_len   = 4,
    .mt_queue_len   = 4,
  };
  const char *msg = "HOLA";

  isbd_setup( &isbd_config );

  // k_msleep( 5000 );

  isbd_msg_enqueue( msg, 2, 4 ); 

}
