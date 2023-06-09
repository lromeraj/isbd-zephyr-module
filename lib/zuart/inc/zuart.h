#ifndef ZUART_H_
  #define ZUART_H_
  
  #include <stdint.h>
  #include <zephyr/sys/ring_buffer.h>

  #define ZUART_CONF_DEFAULT( _dev ) \
    { \
      .dev = _dev, \
      .rx_buf = NULL, \
      .rx_buf_size = 0, \
      .tx_buf = NULL, \
      .tx_buf_size = 0, \
      .mode = ZUART_MODE_POLL, \
    }
  
  #define ZUART_CONF_POLL( _dev ) \
    { \
      .dev = _dev, \
      .rx_buf = NULL, \
      .rx_buf_size = 0, \
      .tx_buf = NULL, \
      .tx_buf_size = 0, \
      .mode = ZUART_MODE_POLL, \
    }

  #define ZUART_CONF_IRQ( _dev, _rx_buf, _rx_buf_size, _tx_buf, _tx_buf_size ) \
    { \
      .dev = _dev, \
      .rx_buf = _rx_buf, \
      .rx_buf_size = _rx_buf_size, \
      .tx_buf = _tx_buf, \
      .tx_buf_size = _tx_buf_size, \
      .mode = ZUART_MODE_IRQ, \
    }

  #define ZUART_CONF_MIX_RX_IRQ_TX_POLL( _dev, _rx_buf, _rx_buf_size ) \
    { \
      .dev = _dev, \
      .rx_buf = _rx_buf, \
      .rx_buf_size = _rx_buf_size, \
      .tx_buf = NULL, \
      .tx_buf_size = 0, \
      .mode = ZUART_MODE_MIXED, \
      .read_proto = zuart_read_irq_proto, \
      .write_proto = zuart_write_poll_proto, \
    }

  #define ZUART_CONF_MIX_RX_POLL_TX_IRQ( _dev, _tx_buf, _tx_buf_size ) \
    { \
      .dev = _dev, \
      .rx_buf = NULL, \
      .rx_buf_size = 0, \
      .tx_buf = _tx_buf, \
      .tx_buf_size = _tx_buf_size, \
      .mode = ZUART_MODE_MIXED, \
      .read_proto = zuart_read_poll_proto, \
      .write_proto = zuart_write_irq_proto, \
    }

  typedef enum zuart_err {
    ZUART_OK              = 0,
    ZUART_ERR             = -1,
    ZUART_ERR_SETUP       = -2,
    ZUART_ERR_OVERRUN     = -3,
    ZUART_ERR_TIMEOUT     = -4,
  } zuart_err_t;

  typedef enum zuart_mode {
    ZUART_MODE_IRQ,
    ZUART_MODE_POLL,
    ZUART_MODE_MIXED,
  } zuart_mode_t;

  typedef struct zuart zuart_t;
  typedef struct zuart_config zuart_config_t;

  typedef uint16_t (*zuart_read_proto_t)(
    zuart_t *zuart, uint8_t *src_buffer, uint16_t n_bytes, uint32_t timeout_ms 
  );

  typedef uint16_t (*zuart_write_proto_t)( 
    zuart_t *zuart, const uint8_t *src_buffer, uint16_t n_bytes, uint32_t timeout_ms 
  );

  struct zuart_config {
    
    zuart_mode_t mode;

    uint8_t *tx_buf;
    uint32_t tx_buf_size;

    uint8_t *rx_buf;
    uint32_t rx_buf_size;
    
    struct device *dev;

    zuart_read_proto_t read_proto;
    zuart_write_proto_t write_proto;

  };

  struct zuart {
    
    uint8_t flags; // for internal use only
    zuart_err_t err; // used to know the last error recorded

    struct device *dev;

    struct k_sem rx_sem; // used based on specific user logic
    struct k_sem tx_sem; // used when the isr has transmitted a chunk

    struct ring_buf rx_rbuf;
    struct ring_buf tx_rbuf;

    zuart_config_t config;
  };

  /**
   * @brief Returns the last recorded error
   * 
   * @param zuart 
   * @return zuart_err_t 
   */
  static inline const zuart_err_t zuart_get_err( zuart_t *zuart ) {
    return zuart->err;
  }

  /**
   * @brief 
   * 
   * @param zuart 
   * @param zuart_config 
   * @return int 
   */
  zuart_err_t zuart_setup( zuart_t *zuart, const zuart_config_t *zuart_config );
  
  /**
   * @brief Request a specific number of bytes from que reception buffer.
   * Use timeout in order to decide if this call should wait before 
   * receiving all requested bytes. This function is not thread safe
   * 
   * @param zuart zuart instance
   * @param bytes Output buffer
   * @param n_bytes Number of bytes to read from the serial port
   * @param ms_timeout The number of milliseconds this function should wait
   * @return int 
   */
  uint16_t zuart_read( zuart_t *zuart, uint8_t *out_buffer, uint16_t n_bytes, uint32_t ms_timeout );

  /**
   * @brief Write a given number of bytes into the transmission buffer.
   * This function is not thread safe
   * 
   * @param zuart 
   * @param bytes 
   * @param n_bytes 
   * @param ms_timeout 
   * @return uint32_t 
   */
  uint16_t zuart_write( zuart_t *zuart, const uint8_t *src_buffer, uint16_t n_bytes, uint32_t ms_timeout );

  /**
   * @brief Purges UART reception buffer
   * 
   * @param zuart instance
   * @return uint32_t Number of purged bytes
   */
  uint32_t zuart_drain( zuart_t *zuart );

  /**
   * @brief Read from serial port using interrupt technique
   * 
   * @param zuart 
   * @param out_buf 
   * @param n_bytes 
   * @param timeout_ms 
   * @return int32_t 
   */
  uint16_t zuart_read_irq_proto( zuart_t *zuart, uint8_t *out_buf, uint16_t n_bytes, uint32_t timeout_ms );
  
  /**
   * @brief Reads from serial port using polling technique
   * 
   * @note: Using this technique implies that the calling thread
   * must be fast enough to pull out every char in the FIFO buffer, 
   * otherwise data loss will ocurre. Use this mode only if your 
   * software/hardware architecture capabilities are enough 
   * to handle the desired baudrate.
   * 
   * @param zuart 
   * @param out_buf 
   * @param n_bytes 
   * @param timeout_ms 
   * @return int32_t 
   */
  uint16_t zuart_read_poll_proto( zuart_t *zuart, uint8_t *out_buf, uint16_t n_bytes, uint32_t timeout_ms );

  /**
   * @brief Write to serial port using interrupts technique
   * 
   * @param zuart 
   * @param src_buf 
   * @param n_bytes 
   * @param timeout_ms 
   * @return int32_t 
   */
  uint16_t zuart_write_irq_proto( zuart_t *zuart, const uint8_t *src_buf, uint16_t n_bytes, uint32_t timeout_ms );
  uint16_t zuart_write_poll_proto( zuart_t *zuart, const uint8_t *src_buf, uint16_t n_bytes, uint32_t timeout_ms );

  void zuart_force_read_timeout( zuart_t *zuart );
  void zuart_force_write_timeout( zuart_t *zuart );

  /**
   * @brief Number of characters available in the reception buffer
   * 
   * @note Only works for interrupt mode, otherwise 0 is always returned
   * 
   * @param zuart 
   * @return uint16_t 
   */
  uint16_t zuart_available( zuart_t *zuart );
  
#endif