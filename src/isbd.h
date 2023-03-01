#ifndef ISBD_H_
  #define ISBD_H_

  #include <stdint.h>

  typedef enum isbd_clear_buffer {
    ISBD_CLEAR_MO_BUFF      = 0,
    ISBD_CLEAR_MT_BUFF      = 1,
    ISBD_CLEAR_MO_MT_BUFF   = 2,
  } isbd_clear_buffer_t;

  struct isbd_config {
    bool echo;
    bool verbose;
    struct device *dev;
  };

  typedef enum isbd_err {
    ISBD_OK        = 0,
    ISBD_ERR,
  } isbd_err_t;

  typedef enum isbd_at_code {
    ISBD_AT_RING        = -1,
    ISBD_AT_TIMEOUT     = -2,
    ISBD_AT_RDY         = -3,
    ISBD_AT_ERR         = -4,
    ISBD_AT_UNK         = -5,
    ISBD_AT_OK          = 0,
  } isbd_at_code_t;

  typedef struct isbd_session {
    uint8_t mo_sts, mt_sts;
    uint16_t mo_msn, mt_msn;
    uint16_t mt_len;
    uint8_t mt_queued;
  } isbd_session_t;

  isbd_err_t isbd_setup( struct isbd_config *config );

  isbd_err_t isbd_test_at();

  /**
   * @brief Echo command characters.
   * 
   * @note If enabled, this is used as an extra check to know 
   * if the device has received correctly the AT command. 
   * This is usually not necessary except in cases
   * where connection is too noisy, and despite of this the AT command will be corrupted
   * and the device will respond with an ERR anyway, so it's recommended to disable
   * echo in order to reduce RX serial traffic.
   * 
   * @param enable 
   * @return int8_t 
   */
  int8_t isbd_enable_echo( bool enable );

  /**
   * @brief Query the device for Iridium system time if available
   * 
   * @param __rtc 
   * @return int8_t 
   */
  int8_t isbd_get_rtc( char *__rtc , uint16_t rtc_len);

  /**
   * @brief Query the device IMEI
   * 
   * @param __imei Resulting IMEI memory buffer
   * @return int8_t
   */          
  int8_t isbd_get_imei( char *__imei, uint16_t imei_len );

  /**
   * @brief Query the device revision
   * 
   * @param __revision 
   * @return int8_t 
   */
  int8_t isbd_get_revision( char *__rev, uint16_t rev_len );

  /**
   * @brief Transfer a SBD text message from the DTE 
   * to the single mobile originated buffer.
   * 
   * @param txt SBD message with a maximum length of 120 bytes (excluding null char)
   * @return isbd_at_code_t 
   */
  int8_t isbd_set_mo_txt( const char *txt );

  /**
   * @brief Transfer a longer SBD text message from the DTE 
   * to the single mobile originated buffer.
   * 
   * @param txt SBD message with a maximum length of 340 bytes (excluding null char)
   * @return isbd_at_code_t 
   */
  // int8_t isbd_set_mo_txt_l( char *txt );

  /**
   * @brief Transfer the contents of the mobile originated buffer 
   * to the mobile terminated buffer.
   * 
   * @param __out ISU string response. Use NULL to ignore the string response
   * @return int8_t 
   */
  int8_t isbd_mo_to_mt( char *__out, uint16_t out_len );

  /**
   * @brief 
   * 
   * @param __msg 
   * @param msg_len 
   * @return int8_t 
   */
  int8_t isbd_get_mt_bin( uint8_t *__msg, uint16_t *msg_len, uint16_t *csum );

  /**
   * @brief 
   * 
   * @param __msg 
   * @param msg_len 
   * @return int8_t 
   */
  int8_t isbd_set_mo_bin( const uint8_t *__msg, uint16_t msg_len );

  /**
   * @brief This command is used to transfer a text SBD message 
   * from the single mobile terminated buffer to the DTE
   * 
   * @note The intent of this function is to provide 
   * a human friendly interface to SBD for demonstrations and application development. 
   * It is expected that most usage of SBD will be with binary messages. @see isbd_get_mt_bin()
   *  
   * @param __mt_buff 
   * @return int8_t 
   */
  int8_t isbd_get_mt_txt( char *__mt_buff, uint16_t mt_buff_len );

  /**
   * @brief 
   * 
   * @param session 
   * @return int8_t 
   */
  int8_t isbd_init_session( isbd_session_t *session );

  /**
   * @brief 
   * 
   * @param session 
   * @return int8_t 
   */
  int8_t isbd_clear_buffer( isbd_clear_buffer_t buffer );

#endif