#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "zuart.h"

#define FLAG_OVERRUN    1 // bit 1

#define SET_FLAG( var, flag ) \
  WRITE_BIT( var, flag, 1 )

#define CLEAR_FLAG( var, flag ) \
  WRITE_BIT( var, flag, 0 )

#define GET_FLAG( var, flag ) \
  ( (var) & BIT( flag ) )

// --------- Start of private methods -----------
/**
 * @brief UART ISR handler
 * 
 * @param dev UART driver instance
 * @param user_data Pointer to the zuart struct
 */
static void _uart_isr( const struct device *dev, void *user_data );
// ---------- End of private methods -----------

// TODO: think about using events

uint16_t zuart_read_irq_proto( zuart_t *zuart, uint8_t *out_buf, uint16_t n_bytes, uint32_t timeout_ms ) {
  
  int sem_ret = 0;
  uint16_t total_bytes_read = 0;

  // this conversion takes a little due to arithmetic division
  // so we temporary store the value instead of recomputing for each loop
  k_timeout_t k_timeout = K_MSEC( timeout_ms );

  while ( total_bytes_read < n_bytes ) {
    
    if ( ring_buf_is_empty( &zuart->rx_rbuf ) ) {
      sem_ret = k_sem_take( &zuart->rx_sem, k_timeout );
      if ( sem_ret < 0 ) break;
    }

    total_bytes_read += ring_buf_get(
      &zuart->rx_rbuf, out_buf + total_bytes_read, n_bytes - total_bytes_read );

  }

  // ! This is causing data loss, please see: (FIXED) 
  // ! https://glab.lromeraj.net/ucm/miot/tfm/iridium-sbd-library/-/issues/11
  // while ( total_bytes_read < n_bytes
  //   && ( sem_ret = k_sem_take( &zuart->rx_sem, k_timeout ) ) == 0
  //   && ( (bytes_read = ring_buf_get(
  //     // ! We have have to take care ONLY if concurrent reads are a possibility,
  //     // ! at least we should warn to the user
  //     &zuart->rx_rbuf, 
  //     out_buf + total_bytes_read, 
  //     n_bytes - total_bytes_read ) ) > 0 )
  // ) {
  //   total_bytes_read += bytes_read;
  // }

  if ( GET_FLAG( zuart->flags, FLAG_OVERRUN ) ) {
    zuart->err = ZUART_ERR_OVERRUN;
    CLEAR_FLAG( zuart->flags, FLAG_OVERRUN );
  } else if ( timeout_ms > 0 && sem_ret ) {
    zuart->err = ZUART_ERR_TIMEOUT;
  }

  return total_bytes_read;
}

uint16_t zuart_read_poll_proto( zuart_t *zuart, uint8_t *out_buf, uint16_t n_bytes, uint32_t timeout_ms ) {

  uint8_t byte;
  uint16_t total_bytes_read = 0;

  if ( timeout_ms == 0 ) {

    while ( total_bytes_read < n_bytes 
      && uart_poll_in( zuart->dev, &byte ) == 0 ) {

      out_buf[ total_bytes_read ] = byte;
      total_bytes_read++;
    }

  } else {

    uint64_t ts_old = k_uptime_get();

    while ( total_bytes_read < n_bytes ) {
      
      uint64_t ts_now = k_uptime_get();

      if ( ts_now - ts_old >= timeout_ms ) {
        zuart->err = ZUART_ERR_TIMEOUT;
        break;
      }

      int ret = uart_poll_in( zuart->dev, &byte );

      if ( ret == 0 ) {
        out_buf[ total_bytes_read ] = byte;
        total_bytes_read++;
      } else if ( ret == -1 ) {
        k_yield();
      } else {
        zuart->err = ZUART_ERR;
        break;
      }

    }

  }

  return total_bytes_read; 
}

uint16_t zuart_read(
  zuart_t *zuart, uint8_t *out_buf, uint16_t n_bytes, uint32_t timeout_ms 
) {
  if ( zuart->config.read_proto ) {
    return zuart->config.read_proto( zuart, out_buf, n_bytes, timeout_ms );
  }
  
  zuart->err = ZUART_ERR;
  return 0;
}

uint16_t zuart_write_irq_proto(
  zuart_t *zuart, uint8_t *src_buf, uint16_t n_bytes, uint32_t timeout_ms 
) {
  
  k_sem_reset( &zuart->tx_sem );

  // ! This function is public so the user should take care ONLY 
  // ! if concurrent writes are a possibility
  uint16_t total_bytes_written = 0;
  
  if ( timeout_ms == 0 ) {
    total_bytes_written = ring_buf_put( &zuart->tx_rbuf, src_buf, n_bytes );
    uart_irq_tx_enable( zuart->dev );
  } else {
    
    k_timeout_t k_timeout = K_MSEC( timeout_ms );

    while ( total_bytes_written < n_bytes ) {
      
      if ( ring_buf_space_get( &zuart->tx_rbuf ) == 0 ) {
        if ( k_sem_take( &zuart->tx_sem, k_timeout ) != 0 ) {
          zuart->err = ZUART_ERR_TIMEOUT;
          break;
        }
      }

      total_bytes_written += ring_buf_put(
        &zuart->tx_rbuf, src_buf + total_bytes_written, n_bytes - total_bytes_written );
      
      // enable irq to transmit ring buffer
      uart_irq_tx_enable( zuart->dev );

    }

  }

  return total_bytes_written;
}

uint16_t zuart_write_poll_proto(
  zuart_t *zuart, uint8_t *src_buf, uint16_t n_bytes, uint32_t timeout_ms 
) {

  uint16_t bytes_written = 0;
  uint64_t ts_old = k_uptime_get();

  while ( bytes_written < n_bytes ) {
    
    uint64_t ts_now = k_uptime_get();

    if ( timeout_ms > 0 && ts_now - ts_old >= timeout_ms ) {
      return ZUART_ERR_TIMEOUT;
    }

    uint8_t byte = src_buf[ bytes_written ];
    uart_poll_out( zuart->dev, byte );
    bytes_written++;
  }

  return bytes_written;
}


uint16_t zuart_write( 
  zuart_t *zuart, uint8_t *src_buf, uint16_t n_bytes, uint32_t timeout_ms 
) {

  if ( zuart->config.write_proto ) {
    return zuart->config.write_proto( zuart, src_buf, n_bytes, timeout_ms );
  }

  zuart->err = ZUART_ERR;
  return 0;

}

// TODO: https://glab.lromeraj.net/ucm/miot/tfm/iridium-sbd-library/-/issues/10
void zuart_drain( zuart_t *zuart ) {

  uint32_t size = ring_buf_size_get( &zuart->rx_rbuf );
  
  // purge ring buffer
  ring_buf_get( &zuart->rx_rbuf, NULL, size );
}

zuart_err_t zuart_setup( zuart_t *zuart, const zuart_config_t *zuart_config ) {

  // copy device pointer
  zuart->dev = zuart_config->dev;

  // copy config
  zuart->config = *zuart_config;

  if ( zuart_config->mode == ZUART_MODE_IRQ ) {

    zuart->config.read_proto = zuart_read_irq_proto;
    zuart->config.write_proto = zuart_write_irq_proto;

  } else if ( zuart_config->mode == ZUART_MODE_POLL ) {

    zuart->config.read_proto = zuart_read_poll_proto;
    zuart->config.write_proto = zuart_write_poll_proto;
  }

  if ( zuart->config.read_proto == zuart_read_irq_proto 
      || zuart->config.write_proto == zuart_write_irq_proto ) {
    
    uart_irq_callback_user_data_set( zuart->dev, _uart_isr, zuart );
  }

  if ( zuart->config.read_proto == zuart_read_irq_proto ) {

    if ( zuart->config.rx_buf == NULL || zuart->config.rx_buf_size == 0 ) {
      return ZUART_ERR_SETUP;
    }

    k_sem_init(
      &zuart->rx_sem, 0, 1 );

    ring_buf_init(
      &zuart->rx_rbuf, zuart->config.rx_buf_size, zuart->config.rx_buf );    
    
    uart_irq_rx_enable( zuart->dev );
  } 

  if ( zuart->config.write_proto == zuart_write_irq_proto ) {

    if ( zuart->config.tx_buf == NULL || zuart->config.tx_buf_size == 0 ) {
      return ZUART_ERR_SETUP;
    }

    k_sem_init(
      &zuart->tx_sem, 0, 1 );

    ring_buf_init(
      &zuart->tx_rbuf, zuart->config.tx_buf_size, zuart->config.tx_buf );
  }
  
  return ZUART_OK;
}

static inline void _uart_tx_isr( const struct device *dev, zuart_t *zuart ) {

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

    // is theoretically impossible to receive value lower than 1
    // due to the previous check using uart_irq_tx_ready(), 
    // otherwise is a driver fault
    
    // TODO: we could add extra logic here and give semaphore only if
    // TODO: there are at least N bytes of free space in the ring buffer
    // the semaphore will be given anyway to allow writing bytes
    k_sem_give( &zuart->tx_sem );

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

static inline void _uart_rx_isr( const struct device *dev, zuart_t *zuart ) {
  
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