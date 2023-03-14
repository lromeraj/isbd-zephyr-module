#ifndef ZUART_H_
  #define ZUART_H_
  
  #include <stdint.h>
  #include <zephyr/sys/ring_buffer.h>

  struct zuart_buf {
    uint8_t *rx;
    int rx_len, rx_size;

    uint8_t *tx;
    int tx_len, tx_idx, tx_size;
  };

  typedef struct zuart_config {
    
    struct device *dev;
    
    uint8_t *tx_buf;
    int tx_buf_size;

    uint8_t *rx_buf;
    int rx_buf_size;

  } zuart_config_t;

  typedef struct zuart {
    struct device *dev;
    struct zuart_buf buf; 
    
    struct k_sem rx_sem; // used based on specific user logic
    struct k_sem tx_sem; // used when the isr has transmitted a chunk

    struct k_msgq rx_queue;

    struct ring_buf rx_rbuf;
    struct ring_buf tx_rbuf;

    // TODO: ring buffer for transmission too

    zuart_config_t config;
  } zuart_t;

  int zuart_setup( zuart_t *zuart, zuart_config_t *zuart_config );
  
  /**
   * @brief Allows to request a specific number of bytes from que reception queue
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
  int zuart_read( zuart_t *zuart, uint8_t *bytes, int n_bytes, uint16_t ms_timeout );
  uint32_t zuart_write( zuart_t *zuart, uint8_t *bytes, int n_bytes, uint16_t ms_timeout );

  void zuart_drain( zuart_t *zuart );

#endif