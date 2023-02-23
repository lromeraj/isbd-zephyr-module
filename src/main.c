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

#define MSG_SIZE 32

#define STACK_SIZE		2048
#define PRIORITY		5

/* change this to any other UART peripheral if desired */
#define UART_MASTER_DEVICE_NODE DT_NODELABEL(uart0)
#define UART_SLAVE_DEVICE_NODE DT_NODELABEL(uart3)

static char g_at_buf[256];

K_SEM_DEFINE(g_at_sem_res, 0, 1);
K_SEM_DEFINE(g_at_sem_rdy, 0, 1);

void master_serial_entry( void*, void*, void* );
void slave_serial_entry( void*, void*, void* );

K_THREAD_DEFINE(slave_tid, STACK_SIZE,
                slave_serial_entry, NULL, NULL, NULL,
                PRIORITY, 0, 0);

K_THREAD_DEFINE(master_tid, STACK_SIZE,
                master_serial_entry, NULL, NULL, NULL,
                PRIORITY, 0, 0);

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_master_msgq, MSG_SIZE, 10, 4);
K_MSGQ_DEFINE(uart_slave_msgq, MSG_SIZE, 10, 4);

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

	uart_irq_rx_enable(uart_dev);

}

void master_serial_entry( void *p1, void *p2, void *p3 ) {

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

void at_write_cmd( const char *cmd ) {
	char buf[256];
	sprintf( buf, "at%s\r\n", cmd );
	print_uart( uart_slave_device, buf );
}



void at_write_ext_pcmd( const char *cmd, const char *params ) {
	
	char buf[256];

	if ( params ) {
		sprintf( buf, "+%s%s", cmd, params );		
	} else {
		sprintf( buf, "+%s", cmd );
	}

	at_write_cmd( buf );
}

void at_write_ext_cmd( const char *cmd ) {
	at_write_ext_pcmd( cmd, NULL );
}

void at_write_ext_wcmd( const char *cmd, const char *params ) {
	char buf[256];
	sprintf( buf, "=%s", params );
	at_write_ext_pcmd( cmd, buf );
}

void at_write_ext_ecmd( const char *cmd, uint8_t param ) {
	char buf[4];
	sprintf( buf, "%d", param );
	at_write_ext_pcmd( cmd, buf );
}


const char * sbd_fetch_imei() {

	at_write_ext_cmd( "cgsn" );

	if ( k_sem_take(&g_at_sem_res, K_FOREVER) == 0 ) {
		return g_at_buf;	
	}
	return NULL;
}

void slave_serial_entry( void *p1, void *p2, void *p3 ) {

	char tx_buf[MSG_SIZE];
	struct uart_config config;

	uart_config_get( uart_slave_device, &config );
	config.baudrate = 19200;
	uart_configure( uart_slave_device, &config );

	setup_uart( uart_slave_device, &slave_rx_buff, &uart_slave_msgq );

	k_sem_give( &g_at_sem_rdy );

	while (k_msgq_get(&uart_slave_msgq, &tx_buf, K_FOREVER) == 0) {
		
		print_uart( uart_master_device, "9602 -> Host: " );
		print_uart( uart_master_device, tx_buf );
		print_uart( uart_master_device, "\r\n");

		if ( strcmp( tx_buf, "OK" ) == 0 ) {
			k_sem_give( &g_at_sem_res );
		} else {
			strcpy( g_at_buf, tx_buf );
		}

	}


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

	print_uart( uart_master_device, "Hello! I'm your echo bot for Iridium 9602 module.\r\n\r\n" );

	// initialization should not use a sempahore, use an internal initialiaztion flag instead
	if ( k_sem_take( &g_at_sem_rdy, K_FOREVER ) == 0 ) {
		printk("READY ...");
		while (1) {
			
			// this is a blocking call
			const char *imei = sbd_fetch_imei();

			printk("Device IMEI: %s\n", imei );
			k_msleep( 500 );
		}
	}


}
