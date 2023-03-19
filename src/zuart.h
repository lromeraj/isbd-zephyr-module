#ifndef ZUART_H_
  #define ZUART_H_
  
  #include <stdint.h>
  #include <zephyr/sys/ring_buffer.h>

  typedef enum zuart_err {
    ZUART_OK = 0,
    ZUART_ERR_SETUP,
    ZUART_ERR_OVERRUN,
    ZUART_ERR_TIMEOUT,
  } zuart_err_t;

  typedef struct zuart_config {

    uint8_t *tx_buf;
    uint32_t tx_buf_size;

    uint8_t *rx_buf;
    uint32_t rx_buf_size;

    struct device *dev;

  } zuart_config_t;

  typedef struct zuart {
    
    uint8_t flags; // for internal use only

    struct device *dev;

    struct k_sem rx_sem; // used based on specific user logic
    struct k_sem tx_sem; // used when the isr has transmitted a chunk

    struct ring_buf rx_rbuf;
    struct ring_buf tx_rbuf;

    zuart_config_t config;

  } zuart_t;

  /**
   * @brief 
   * 
   * @param zuart 
   * @param zuart_config 
   * @return int 
   */
  int zuart_setup( zuart_t *zuart, zuart_config_t *zuart_config );
  
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

#endif