#ifndef ZUART_H_
  #define ZUART_H_
  
  #include <stdint.h>

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
    struct k_msgq rx_queue;
    zuart_config_t config;
  } zuart_t;

  int zuart_setup( zuart_t *zuart, zuart_config_t *zuart_config );
  
  int zuart_write_sync( zuart_t *zuart, uint8_t *bytes, int n_bytes, uint16_t ms_timeout );
  int zuart_write_async( zuart_t *zuart, uint8_t *bytes, int n_bytes );
  
  int zuart_read_sync( zuart_t *zuart, uint8_t *bytes, int n_bytes, uint16_t ms_timeout );
  int zuart_read_async( zuart_t *zuart, uint8_t *bytes, int n_bytes );

  void zuart_rx_purge( zuart_t *zuart );

#endif