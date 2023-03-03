#ifndef ISBD_DEFS_H_
  
  #define ISBD_DEFS_H_

  #define RX_MSGQ_LEN     256

  // TODO: think in reducing buffer sizes
  #define TX_BUFF_SIZE    512 
  #define RX_BUFF_SIZE    128

  typedef enum isbd_err {
    ISBD_OK        = 0,
    ISBD_ERR,
  } isbd_err_t;

#endif