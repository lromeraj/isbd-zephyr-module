#ifndef ISBD_H_
  #define ISBD_H_

  #include <stdint.h>

  struct isbd_config {
    bool verbose;
    struct device * dev;
  };

  typedef enum isbd_err {
    ISBD_OK        = 0,
    ISBD_ERR,
  } isbd_err_t;

  typedef enum isbd_at_err {
    ISBD_AT_UNK    = -1,
    ISBD_AT_OK     = 0,
    ISBD_AT_ERR,
    ISBD_AT_RDY,
    ISBD_AT_RING,
  } isbd_at_code_t;

  isbd_err_t isbd_setup( struct isbd_config *config );

  isbd_err_t isbd_test_at();
  isbd_at_code_t isbd_fetch_rtc( char *__rtc );
  isbd_at_code_t isbd_fetch_imei( char *__imei );
  isbd_at_code_t isbd_fetch_revision( char *__revision );

  // isbd_at_code_t isbd_enable_flow_control( bool enable );

  /**
   * @brief Transfer a text SBD message from the DTE 
   * to the single mobile originated buffer.
   * 
   * @param txt SBD message with a maximum length of 119 bytes (excluding null char)
   * @return isbd_at_code_t 
   */
  isbd_at_code_t isbd_set_mo_txt( const char *txt );

  /**
   * @brief Transfer a long text SBD message from the DTE 
   * to the single mobile originated buffer.
   * 
   * @param txt SBD message with a maximum length of 340 bytes (excluding null char)
   * @return isbd_at_code_t 
   */
  isbd_at_code_t isbd_set_mo_txt_l( const char *txt );

  /**
   * @brief Transfer the contents of the mobile originated buffer 
   * to the mobile terminated buffer.
   * 
   * @param __out ISU string response. Use NULL to ignore the string response
   * @return isbd_at_code_t 
   */
  isbd_at_code_t isbd_mo_to_mt( char *__out );

  /**
   * @brief This command is used to transfer a text SBD message 
   * from the single mobile terminated buffer to the DTE
   * 
   * @note The intent of this function is to provide 
   * a human friendly interface to SBD for demonstrations and application development. 
   * It is expected that most usage of SBD will be with binary messages. @see isbd_get_mt_bin()
   *  
   * @param __mt_buff 
   * @return isbd_at_code_t 
   */
  isbd_at_code_t isbd_get_mt_txt( char *__mt_buff );

#endif