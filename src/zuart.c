#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
  
#include "zuart.h"

static void _uart_isr( const struct device *dev, void *user_data );


int zuart_write_sync( zuart_t *zuart, uint8_t *bytes, int n_bytes, uint16_t timeout_ms ) {
  return -1;
}

int zuart_read_sync( zuart_t *zuart, uint8_t *bytes, int n_bytes, uint16_t timeout_ms ) {

  uint8_t byte;
  int n_bytes_read = 0;

  // this conversion takes a little due to arithmetic division
  // so we temporary store the value instead of recomputing for each loop
  k_timeout_t k_timeout = K_MSEC( timeout_ms );

  while ( n_bytes_read < n_bytes
    && k_msgq_get( &zuart->rx_queue, &byte, k_timeout ) == 0 ) {
    if ( bytes ) {
      *bytes++ = byte;
    }
    n_bytes_read++;
  }

  return n_bytes_read;
}

int zuart_write_async( zuart_t *zuart, uint8_t *bytes, int n_bytes ) {

  n_bytes = n_bytes > zuart->buf.tx_size 
    ? zuart->buf.tx_size 
    : n_bytes;

  zuart->buf.tx_idx = 0;
  zuart->buf.tx_len = n_bytes;

  bytecpy( zuart->buf.tx, bytes, n_bytes );

  uart_irq_tx_enable( zuart->dev );

  return n_bytes;
}

void zuart_rx_purge( zuart_t *zuart ) {
  k_msgq_purge( &zuart->rx_queue );
}

int zuart_read_async( zuart_t *zuart, uint8_t *bytes, int n_bytes ) {
  return -1;
}

int zuart_setup( zuart_t *zuart, zuart_config_t *zuart_config ) {
  
  // driver instance
  zuart->dev = zuart_config->dev;
  
  // rx buffer
  zuart->buf.rx = zuart_config->rx_buf;
  zuart->buf.rx_size = zuart_config->rx_buf_size;

  // tx buffer
  zuart->buf.tx = zuart_config->tx_buf;
  zuart->buf.tx_size = zuart_config->tx_buf_size;
  zuart->buf.tx_idx = 0;
  zuart->buf.tx_len = 0;

  #ifdef CONFIG_UART_INTERRUPT_DRIVEN

    // initialize message queue
    k_msgq_init( 
      &zuart->rx_queue,
      zuart->buf.rx,
      sizeof( uint8_t ),
      zuart->buf.rx_size / sizeof( uint8_t ) );

    uart_irq_callback_user_data_set( 
      zuart->dev, _uart_isr, zuart );
    
    /*
    if (ret < 0) {
      if (ret == -ENOTSUP) {
        printk("Interrupt-driven UART API support not enabled\n");
      } else if (ret == -ENOSYS) {
        printk("UART device does not support interrupt-driven API\n");
      } else {
        printk("Error setting UART callback: %d\n", ret);
      }
    }
    */

    uart_irq_rx_enable( zuart->dev );
    uart_irq_tx_disable( zuart->dev );

    return 0;

  #else
    return -1;
  #endif

}

static void _uart_tx_isr( const struct device *dev, zuart_t *zuart ) {

  uint8_t *tx_buff = zuart->buf.tx;
  size_t *tx_buff_len = &zuart->buf.tx_len;
  size_t *tx_buff_idx = &zuart->buf.tx_idx;
  
  if ( *tx_buff_idx < *tx_buff_len ) {
    
    (*tx_buff_idx) += uart_fifo_fill(
      dev, &tx_buff[ *tx_buff_idx ], *tx_buff_len - *tx_buff_idx );

  } else {
  
    // ! This may cause _uart_tx_isr() to be unnecessary over-called
    // ! This has been fixed by allowing interrupt to exit multiple times
    // ! instead of blocking for one call
    // while ( !uart_irq_tx_complete( dev ) );

    if ( uart_irq_tx_complete( dev ) ) {

      // *tx_buff_idx = 0;
      // *tx_buff_len = 0;
      
      // ! When uart_irq_tx_disable() is called
      // ! the transmission is halted although 
      // ! the fifo was filled successfully
      // ! See: https://github.com/zephyrproject-rtos/zephyr/issues/10672
      uart_irq_tx_disable( dev );
    }

  }

}

static void _uart_rx_isr( const struct device *dev, zuart_t *zuart ) {
  
  uint8_t byte;
  
  if ( uart_fifo_read( dev, &byte, 1 ) == 1 ) {
    
    int ret = k_msgq_put( &zuart->rx_queue, &byte, K_NO_WAIT );
    
    if ( ret !=  0 ) {
      // set OVERRUN flag
    } 

  }
}

static void _uart_isr( const struct device *dev, void *user_data ) {

	if ( !uart_irq_update( dev ) ) {
		return;
	}

  zuart_t *zuart = (zuart_t*)user_data;

  if ( uart_irq_rx_ready( dev ) ) {
    _uart_rx_isr( dev, zuart );
  }

  if ( uart_irq_tx_ready( dev ) ) {
    _uart_tx_isr( dev, zuart );
  }

}