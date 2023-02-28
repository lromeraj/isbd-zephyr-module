/*
 * Copyright (c) 2022 Libre Solar Technologies GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/devicetree.h>

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "at.h"
#include "isbd.h"

#define MSG_SIZE 32

#define STACK_SIZE		2048
#define PRIORITY		  5

/* change this to any other UART peripheral if desired */
#define UART_MASTER_DEVICE_NODE DT_NODELABEL(uart0)
#define UART_SLAVE_DEVICE_NODE DT_NODELABEL(uart3)

void master_serial_entry( void*, void*, void* );

K_THREAD_DEFINE(master_tid, STACK_SIZE,
                master_serial_entry, NULL, NULL, NULL,
                PRIORITY, 0, 0);

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_master_msgq, MSG_SIZE, 10, 4);

static const struct device *const uart_master_device = DEVICE_DT_GET(UART_MASTER_DEVICE_NODE); 
static const struct device *const uart_slave_device = DEVICE_DT_GET(UART_SLAVE_DEVICE_NODE);

struct serial_rx_buff {
	int pos;
	char buff[MSG_SIZE];
	struct k_msgq *msgq;
};

static struct serial_rx_buff slave_rx_buff;
static struct serial_rx_buff master_rx_buff;

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;
	struct serial_rx_buff *rx_buff = (struct serial_rx_buff*)user_data;

	if (!uart_irq_update(dev)) {
		return;
	}

	if (!uart_irq_rx_ready(dev)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(dev, &c, 1) == 1) {

		if (c == '\n' || c == '\r') {
			
			/* terminate string */
			rx_buff->buff[rx_buff->pos] = '\0';

			if ( rx_buff->pos > 0 ) {
				/* if queue is full, message is silently dropped */
				k_msgq_put(rx_buff->msgq, &rx_buff->buff, K_NO_WAIT);
			}
			
			/* reset the buffer (it was copied to the msgq) */
			rx_buff->pos = 0;
		} else if (rx_buff->pos < (MSG_SIZE - 1)) {
			rx_buff->buff[rx_buff->pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}
}

/*
 * Print a null-terminated string character by character to the UART interface
 */
void print_uart( const struct device *uart_dev, char *buf)
{
	int msg_len = strlen(buf);
	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart_dev, buf[i]);
	}
}

void setup_uart( const struct device *uart_dev, struct serial_rx_buff *rx_buff, struct k_msgq *msgq ) {
	
	rx_buff->msgq = msgq;

	int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, rx_buff);

	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return;
	}
	
  uart_irq_rx_enable( uart_dev );

}

void master_serial_entry( void *p1, void *p2, void *p3 ) {

  return;
  
	char tx_buf[MSG_SIZE];
	struct uart_config config;

	uart_config_get( uart_master_device, &config );
	config.baudrate = 115200;
	uart_configure( uart_master_device, &config );

	setup_uart( uart_master_device, &master_rx_buff, &uart_master_msgq );

	/* indefinitely wait for input from the master message queue */
	while (k_msgq_get(&uart_master_msgq, &tx_buf, K_FOREVER) == 0) {

		print_uart( uart_slave_device, tx_buf );
		print_uart( uart_slave_device, "\r\n" );

		print_uart( uart_master_device, "Host -> 9602: " );
		print_uart( uart_master_device, tx_buf );
		print_uart( uart_master_device, "\r\n" );
	}

}

void check_err( isbd_err_t err ) {
  if ( err != 0 ) { 
    printk( "Command failed: %d\n", err ); 
  }
}

#define CHECK_AT_CMD( code, ok_block, f_name, ... ) \
  code = f_name( __VA_ARGS__ ); \
  if ( code == 0 ) { \
    printk( "%-20s() OK; ", #f_name ); \
    ok_block \
    printk( "\n" ); \
  } else { \
    printk( "%-20s() ERR: %hhd\n", #f_name, code ); \
  }

void main(void) {

	if (!device_is_ready(uart_master_device)) {
		printk("UART master device not found!");
		return;
	}

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
    .echo = true,
    .verbose = false,
    .dev = uart_slave_device,
  };

  char __buff[ 256 ];

  int8_t code;
  isbd_setup( &isbd_config );

  // isbd_fetch_imei( __buff, sizeof( __buff ) );

  // CHECK_AT_CMD( code, {}, isbd_enable_echo, true );

  CHECK_AT_CMD( code, {
    printk( "IMEI : %s", __buff );
  }, isbd_fetch_imei, __buff, sizeof( __buff ) );  
  
  const char *msg = "Hi!";

  CHECK_AT_CMD( code, {}, isbd_set_mo_bin, msg, strlen( msg ) );
  
  CHECK_AT_CMD( code, {
    printk( "%s", __buff );
  }, isbd_mo_to_mt, __buff, sizeof( __buff ) );

  uint16_t len, csum;
  CHECK_AT_CMD( code, {
    printk("msg=");
    for ( int i=0; i < len; i++ ) {
      printk( "%c", __buff[ i ] );
    }
    printk( ", len=%d, csum=%04X", len, csum );
  }, isbd_get_mt_bin, __buff, &len, &csum ); 
  
 
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
  
  /*
  printk( "Starting session ...\n" );

  isbd_session_t session;
  isbd_init_session( &session );

  printk( "MO Status   : %hhu,\n"
          "MO MSN      : %hu,\n"
          "MT Status   : %hhu,\n"
          "MT MSN      : %hu,\n"
          "MT Length   : %hu,\n"
          "MT Queued   : %hu\n",
    session.mo_sts,
    session.mo_msn,
    session.mt_sts,
    session.mt_msn,
    session.mt_len,
    session.mt_queued );

    */

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
