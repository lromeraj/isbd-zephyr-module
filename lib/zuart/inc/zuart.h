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

  typedef int32_t (*zuart_read_proto_t)(
    zuart_t *zuart, uint8_t *src_buffer, uint16_t n_bytes, uint16_t timeout_ms 
  );

  typedef int32_t (*zuart_write_proto_t)( 
    zuart_t *zuart, uint8_t *src_buffer, uint16_t n_bytes, uint16_t timeout_ms 
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

    struct device *dev;

    struct k_sem rx_sem; // used based on specific user logic
    struct k_sem tx_sem; // used when the isr has transmitted a chunk

    struct ring_buf rx_rbuf;
    struct ring_buf tx_rbuf;

    zuart_config_t config;

  };

  /**
   * @brief 
   * 
   * @param zuart 
   * @param zuart_config 
   * @return int 
   */
  zuart_err_t zuart_setup( zuart_t *zuart, const zuart_config_t *zuart_config );
  
  /**
   * @brief Allows to request a specific number of bytes from que reception buffer
   * 
   * @note Use timeout in order to decide if this call should wait 
   * for all the requested bytes
   * 
   * @param zuart zuart instance
   * @param bytes Output buffer
   * @param n_bytes Number of bytes to read from the serial port
   * @param ms_timeout The number of milliseconds this function should wait
   * @return int 
   */
  int32_t zuart_read( zuart_t *zuart, uint8_t *out_buffer, uint16_t n_bytes, uint16_t ms_timeout );

  /**
   * @brief 
   * 
   * @param zuart 
   * @param bytes 
   * @param n_bytes 
   * @param ms_timeout 
   * @return uint32_t 
   */
  int32_t zuart_write( zuart_t *zuart, uint8_t *src_buffer, uint16_t n_bytes, uint16_t ms_timeout );

  /**
   * @brief 
   * 
   * @param zuart 
   */
  void zuart_drain( zuart_t *zuart );


  int32_t zuart_read_irq_proto( zuart_t *zuart, uint8_t *out_buf, uint16_t n_bytes, uint16_t timeout_ms );
  int32_t zuart_read_poll_proto( zuart_t *zuart, uint8_t *out_buf, uint16_t n_bytes, uint16_t timeout_ms );

  int32_t zuart_write_irq_proto( zuart_t *zuart, uint8_t *src_buf, uint16_t n_bytes, uint16_t timeout_ms );
  int32_t zuart_write_poll_proto( zuart_t *zuart, uint8_t *src_buf, uint16_t n_bytes, uint16_t timeout_ms );

#endif