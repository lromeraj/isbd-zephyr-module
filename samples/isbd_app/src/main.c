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

#include "shared.h"

LOG_MODULE_REGISTER( app );

// LOG_MODULE_REGISTER( isu_sbd_cmds, LOG_LEVEL_DBG );

#include "isbd.h"

#define DO_FOREVER while (1)

#define MO_MSG_RETRIES 8

#ifdef CONFIG_BOARD_FRDM_K64F

  #define LED0_NODE DT_ALIAS( led0 )
  #define LED1_NODE DT_ALIAS( led1 )
  #define LED2_NODE DT_ALIAS( led2 )

  static const struct gpio_dt_spec red_led    = GPIO_DT_SPEC_GET( LED2_NODE, gpios );
  static const struct gpio_dt_spec blue_led   = GPIO_DT_SPEC_GET( LED1_NODE, gpios );
  static const struct gpio_dt_spec green_led  = GPIO_DT_SPEC_GET( LED0_NODE, gpios );

  #define RED_LED &red_led
  #define BLUE_LED &blue_led
  #define GREEN_LED &green_led

  #define TURN_ON_LED( led ) \
    gpio_pin_configure_dt( led, GPIO_OUTPUT_ACTIVE );

  #define TURN_OFF_LED( led ) \
    gpio_pin_configure_dt( led, GPIO_OUTPUT_INACTIVE );

#else

  #define RED_LED
  #define BLUE_LED
  #define GREEN_LED

  #define TURN_ON_LED( led )
  #define TURN_OFF_LED( led )

#endif

static isu_dte_t g_isu_dte;
static struct device *uart_960x_device = UART_960X_DEVICE;

void clear_leds() {
  TURN_OFF_LED( BLUE_LED );
  TURN_OFF_LED( GREEN_LED );
  TURN_OFF_LED( RED_LED );
}

void set_warning_led() {
  clear_leds();
  TURN_ON_LED( RED_LED );
  TURN_ON_LED( GREEN_LED );
}

void set_error_led() {
  clear_leds();
  TURN_ON_LED( RED_LED );
}

void set_success_led() {
  clear_leds();
  TURN_ON_LED( GREEN_LED );
}

void set_info_led() {
  clear_leds();
  TURN_ON_LED( BLUE_LED );
}

static inline void _isbd_evt_handler( isbd_evt_t *evt );

static uint8_t rx_buf[ 512 ];
static uint8_t tx_buf[ 512 ];

int main(void) {

	if ( !device_is_ready( uart_960x_device ) ) {
		LOG_ERR( "UART device not found" );
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
      // .zuart = ZUART_CONF_POLL( uart_960x_device ),
      .zuart = ZUART_CONF_IRQ( uart_960x_device, rx_buf, sizeof( rx_buf ), tx_buf, sizeof( tx_buf ) ),
      // .zuart = ZUART_CONF_MIX_RX_IRQ_TX_POLL( uart_960x_device, rx_buf, sizeof( rx_buf ) ),
      // .zuart = ZUART_CONF_MIX_RX_POLL_TX_IRQ( uart_960x_device, tx_buf, sizeof( tx_buf ) ),
    }
  };
  
  

  LOG_INF( "%s", "Setting up DTE ..." );

  if ( isu_dte_setup( &g_isu_dte, &isu_dte_config ) == ISU_DTE_OK ) {
    LOG_INF( "%s", "Modem OK" );
    set_info_led();
  } else {
    LOG_ERR( "%s", "Could not talk to modem, probably busy ..." );
    set_warning_led();
    return 1;
  }

  isbd_config_t isbd_config = 
    ISBD_DEFAULT_CONF( &g_isu_dte );

  LOG_INF( "%s", "Setting up Iridium SBD service ..." );

  isbd_setup( &isbd_config );

  const char *msg = "UCM - MIoT";

  isbd_send_mo_msg( msg, strlen( msg ), MO_MSG_RETRIES );

  DO_FOREVER {
    isbd_evt_t isbd_evt;
    if ( isbd_wait_evt( &isbd_evt, 1000 ) ) {
      _isbd_evt_handler( &isbd_evt );
    }
  }

  return 0;
}

static void _isbd_evt_handler( isbd_evt_t *evt ) {
  
  switch ( evt->id ) {

    case ISBD_EVT_MO:
      LOG_INF( "MO message sent, sn=%u", evt->mo.sn );
      break;

    case ISBD_EVT_MT:
      LOG_INF( "MT message received, sn=%u", evt->mt.sn );
      LOG_HEXDUMP_INF( evt->mt.data, evt->mt.len, "MT Payload" );
      break;

    case ISBD_EVT_RING:
      LOG_INF( "Ring alert received" );
      isbd_request_session( true );
      break;

    case ISBD_EVT_SIGQ:
      LOG_INF( "Signal strength: %d", evt->sigq );
      break;

    case ISBD_EVT_SVCA:
      LOG_INF( "Service availability: %d", evt->svca );
      break;

    case ISBD_EVT_ERR:
      LOG_ERR( "Error (%03d) %s", evt->err, isbd_err_name( evt->err ) );
      break;

    default:
      LOG_WRN( "Unknown event (%03d)", evt->id );
      break;
  }
  
  // ! The user is obliged to call this function,
  // ! otherwise memory leaks are a possibility
  isbd_destroy_evt( evt );

}
