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

// https://community.nxp.com/t5/Kinetis-Microcontrollers/My-k64F-board-just-has-a-bootloader-on-it-now-I-cant-find-a-mbed/m-p/619471

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER( app );

// LOG_MODULE_REGISTER( isu_sbd_cmds, LOG_LEVEL_DBG );

#include "isbd.h"

#define DO_FOREVER while (1)

#define MO_MSG_RETRIES 4

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

static inline void _isbd_evt_handler( isbd_evt_t *evt );

static uint8_t rx_buf[ 512 ];
static uint8_t tx_buf[ 512 ];

void main(void) {

  if ( !gpio_is_ready_dt( &red_led )
      || !gpio_is_ready_dt( &blue_led )
      || !gpio_is_ready_dt( &green_led ) ) {
    
    LOG_ERR( "LED device not found\n" );
    return;
  }

	if ( !device_is_ready( uart_slave_device ) ) {
		LOG_ERR( "UART device not found\n" );
		return;
  }

  struct uart_config uart_config;

	uart_config_get( uart_slave_device, &uart_config );
	uart_config.baudrate = 19200;
	uart_configure( uart_slave_device, &uart_config );

  isu_dte_config_t isu_dte_config = {
    .at_uart = {
      .echo = false,
      .verbose = true,
      // .zuart = ZUART_CONF_POLL( uart_slave_device ),
      .zuart = ZUART_CONF_IRQ( uart_slave_device, rx_buf, sizeof( rx_buf ), tx_buf, sizeof( tx_buf ) ),
      // .zuart = ZUART_CONF_MIX_RX_IRQ_TX_POLL( uart_slave_device, rx_buf, sizeof( rx_buf ) ),
      // .zuart = ZUART_CONF_MIX_RX_POLL_TX_IRQ( uart_slave_device, tx_buf, sizeof( tx_buf ) ),
    }
  };

  LOG_DBG( "%s", "Setting up ISU DTE ..." );

  if ( isu_dte_setup( &g_isu_dte, &isu_dte_config ) == ISU_DTE_OK ) {
    LOG_INF( "%s", "Modem OK" );
    set_info_led();
  } else {
    LOG_ERR( "%s", "Could not talk to modem, probably busy ...\n" );
    set_warning_led();
    return;
  }

  isbd_config_t isbd_config = {
    .dte            = &g_isu_dte,
    .priority       = 0,
    .mo_queue_len   = 4,
    .evt_queue_len  = 8,
  };

  isbd_setup( &isbd_config );

  const char *msg = "I";
  isbd_enqueue_mo_msg( msg, strlen( msg ), MO_MSG_RETRIES );

  DO_FOREVER {

    isbd_evt_t isbd_evt;

    if ( isbd_wait_evt( &isbd_evt, 5000 ) ) {
      _isbd_evt_handler( &isbd_evt );
    }

  }

}

static void _dte_evt_handler( isu_dte_evt_t *evt ) {

  switch ( evt->id ) {

    case ISU_DTE_EVT_RING:
      printk( "Ring alert received\n" );
      isbd_request_mt_msg();
      break;
      
    case ISU_DTE_EVT_SIGQ:
      printk( "Signal strength: %d\n", evt->sigq );
      break;

    case ISU_DTE_EVT_SVCA:
      printk( "Service availability: %d\n", evt->svca );
      break;

    default:
      break;
  }

}

static void _isbd_evt_handler( isbd_evt_t *evt ) {
  
  switch ( evt->id ) {

    case ISBD_EVT_MO:
      printk( "MO message sent, sn=%u\n", evt->mo.sn );
      break;

    case ISBD_EVT_MT:
      
      printk( "MT message received, sn=%u\n", evt->mt.sn );

      for ( int i=0; i < evt->mt.len; i++ ) {
        printk( "%02X ", evt->mt.data[ i ] );
      }
      printk( "\n" );

      break;

    case ISBD_EVT_DTE:
      _dte_evt_handler( &evt->dte );
      break;
    
    case ISBD_EVT_ERR:
      printk( "Error (%03d) %s\n", evt->err, isbd_err_name( evt->err ) );
      break;

    default:
      printk( "Unknown event (%03d)\n", evt->id );
      break;
  }
  
  // ! The user is obliged to call this function,
  // ! otherwise memory leaks are a possibility
  isbd_destroy_evt( evt );

}