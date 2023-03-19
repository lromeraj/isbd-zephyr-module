#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "zuart.h"

#define FLAG_OVERRUN    1 // bit 1

#define SET_FLAG( var, flag ) \
  WRITE_BIT( var, flag, 1 )

#define CLEAR_FLAG( var, flag ) \
  WRITE_BIT( var, flag, 0 )

// --------- Start of private methods -----------
/**
 * @brief UART ISR handler
 * 
 * @param dev UART driver instance
 * @param user_data Pointer to the zuart struct
 */
static void _uart_isr( const struct device *dev, void *user_data );
// ---------- End of private methods -----------

// TODO: we should use a semaphore with a native ring buffer
// TODO: see https://docs.zephyrproject.org/3.2.0/kernel/data_structures/ring_buffers.html
// TODO: the ISR should give the semaphore each time it receives some character,
// TODO: also we can tell to the ISR what logic should follow to give that semaphore,
// TODO: one logic could be, for example, every time a byte is received give the semaphore,
// TODO: each time you find a character like \n give the semaphore, etc ...

// TODO: using a queue is worse because if we want to notify that there are available chars
// TODO: we have to push something on it, also if the requested amount of bytes
// TODO: is small the queue will waste a lot of space in the buffer

int32_t zuart_read(
  zuart_t *zuart, uint8_t *bytes, uint16_t n_bytes, uint16_t timeout_ms 
) {
  
  int sem_ret;
  uint32_t bytes_read = 0;
  uint32_t total_bytes_read = 0;

  // this conversion takes a little due to arithmetic division
  // so we temporary store the value instead of recomputing for each loop
  k_timeout_t k_timeout = K_MSEC( timeout_ms );

  while ( total_bytes_read < n_bytes
    && ( sem_ret = k_sem_take( &zuart->rx_sem, k_timeout ) ) == 0
    && ( (bytes_read = ring_buf_get(
      // ! We have have to take care ONLY if concurrent reads are a possibility,
      // ! at least we should warn to the user
      &zuart->rx_rbuf, 
      bytes + total_bytes_read, 
      n_bytes - total_bytes_read ) ) > 0 )
  ) {
    total_bytes_read += bytes_read;
  }

  if ( zuart->flags & FLAG_OVERRUN ) {
    CLEAR_FLAG( zuart->flags, FLAG_OVERRUN );
    return ZUART_ERR_OVERRUN;
  }

  if ( timeout_ms > 0 && sem_ret ) {
    return ZUART_ERR_TIMEOUT; 
  }

  return total_bytes_read;
}

int32_t zuart_write( 
  zuart_t *zuart, uint8_t *src_buffer, uint16_t n_bytes, uint16_t timeout_ms 
) {

  k_sem_reset( &zuart->tx_sem );

  // ! This function is public so the user should take care ONLY 
  // ! if concurrent writes are a possibility
  uint32_t bytes_written = 0;
  
  if ( timeout_ms == 0 ) {
    bytes_written = ring_buf_put( &zuart->tx_rbuf, src_buffer, n_bytes );
    uart_irq_tx_enable( zuart->dev );
  } else {
    
    k_timeout_t k_timeout = K_MSEC( timeout_ms );

    while ( bytes_written < n_bytes ) {

      uint32_t bytes_read = ring_buf_put( 
        &zuart->tx_rbuf, src_buffer + bytes_written, n_bytes - bytes_written );
      
       // enable irq to transmit given buffer
      uart_irq_tx_enable( zuart->dev );

      if ( k_sem_take( &zuart->tx_sem, k_timeout ) == 0 ) {
        bytes_written += bytes_read;
      } else {
        return ZUART_ERR_TIMEOUT;
      }

    }
  }

  return bytes_written;
}

// used to transfer received bytes to reception queue
void zuart_flush( zuart_t *zuart ) {

}

// all bytes pending in the reception buffer will be drained
void zuart_drain( zuart_t *zuart ) {
  // purge ring buffer
  ring_buf_get( &zuart->rx_rbuf, NULL, 0 );
}

int zuart_setup( zuart_t *zuart, zuart_config_t *zuart_config ) {
  
  // driver instance
  zuart->dev = zuart_config->dev;
  zuart->config = *zuart_config;

  #ifdef CONFIG_UART_INTERRUPT_DRIVEN

    uart_irq_callback_user_data_set( 
      zuart->dev, _uart_isr, zuart );

    k_sem_init(
      &zuart->rx_sem, 0, 1 );

    k_sem_init(
      &zuart->tx_sem, 0, 1 );

    ring_buf_init(
      &zuart->rx_rbuf, zuart_config->rx_buf_size, zuart_config->rx_buf );    

    ring_buf_init(
      &zuart->tx_rbuf, zuart_config->tx_buf_size, zuart_config->tx_buf );

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

    return ZUART_OK;

  #else
    return ZUART_ERR_SETUP;
  #endif

}

static void _uart_tx_isr( const struct device *dev, zuart_t *zuart ) {

  // uint8_t *tx_buff = zuart->buf.tx;
  // size_t *tx_buff_len = &zuart->buf.tx_len;
  // size_t *tx_buff_idx = &zuart->buf.tx_idx;
  
  if ( ring_buf_size_get( &zuart->tx_rbuf ) > 0 ) {
    
    uint8_t byte;

    // get byte from ring buffer
    ring_buf_get( &zuart->tx_rbuf, &byte, 1 );

    // ! We don't have a function to know the remaining space
    // ! of the uart fifo, we only know that there is enough space for at least one byte

    // send byte to uart fifo
    uart_fifo_fill( dev, &byte, 1 ); 

    // is practically impossible to receive value lower than 1
    // due to the previous check using uart_irq_tx_ready(), 
    // otherwise is a driver fault

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

      k_sem_give( &zuart->tx_sem );
    }
    
  }
}

static void _uart_rx_isr( const struct device *dev, zuart_t *zuart ) {
  
  uint8_t byte;
  
  if ( uart_fifo_read( dev, &byte, sizeof( byte ) ) == sizeof( byte ) ) {
    
    if ( ring_buf_put( &zuart->rx_rbuf, &byte, sizeof( byte ) ) == 0 ) {
      SET_FLAG( zuart->flags, FLAG_OVERRUN );
    }

    // TODO: we could give semaphore depending on some additional logic
    k_sem_give( &zuart->rx_sem );
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