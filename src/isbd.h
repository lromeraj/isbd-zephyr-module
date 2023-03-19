#ifndef ISBD_H_
  #define ISBD_H_

  #include <stdint.h>

  #include "at_uart.h"

  typedef enum isbd_err {
    ISBD_OK        = 0,
    ISBD_ERR,
  } isbd_err_t;

  typedef enum isbd_clear_buffer {

    /**
     * @brief  Clear the mobile originated buffer
     */
    ISBD_CLEAR_MO_BUFF      = 0,

    /**
     * @brief Clear the mobile terminated buffer
     */
    ISBD_CLEAR_MT_BUFF      = 1,

    /**
     * @brief Clear both the mobile originated and mobile terminated buffers
     */
    ISBD_CLEAR_MO_MT_BUFF   = 2, 

  } isbd_clear_buffer_t;

  struct isbd_config {
    struct at_uart_config at_uart;
  };

  typedef struct isbd_session_ext {
    uint8_t mo_sts, mt_sts;
    uint16_t mo_msn, mt_msn;
    uint16_t mt_len;
    uint8_t mt_queued;
  } isbd_session_ext_t;

  typedef struct isbd_evt_report {
    uint8_t mode;
    uint8_t signal;
    uint8_t service;
  } isbd_evt_report_t;

  isbd_err_t isbd_setup( struct isbd_config *config );

  /**
   * @brief Query the device for Iridium system time if available
   * 
   * @param __rtc 
   * @return int8_t 
   */
  int8_t isbd_get_rtc( char *__rtc , size_t rtc_len );

  /**
   * @brief Query the device IMEI
   * 
   * @param __imei Resulting IMEI memory buffer
   * @return int8_t
   */          
  int8_t isbd_get_imei( char *__imei, size_t imei_len );

  /**
   * @brief Query the device revision
   * 
   * @param __revision 
   * @return int8_t 
   */
  int8_t isbd_get_revision( char *__rev, size_t rev_len );

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
  int8_t isbd_mo_to_mt( char *__out, size_t out_len );

  /**
   * @brief 
   * 
   * @param __msg 
   * @param msg_len 
   * @return int8_t 
   */
  int8_t isbd_get_mt( uint8_t *__msg, size_t *msg_len, uint16_t *csum );

  /**
   * @brief 
   * 
   * @param __msg 
   * @param msg_len 
   * @return int8_t 
   */
  int8_t isbd_set_mo( const uint8_t *__msg, size_t msg_len );

  /**
   * @brief This command is used to transfer a text SBD message 
   * from the single mobile terminated buffer to the DTE
   * 
   * @note The intent of this function is to provide 
   * a human friendly interface to SBD for demonstrations and application development. 
   * It is expected that most usage of SBD will be with binary messages.
   *  
   * @param __mt_buff 
   * @return int8_t 
   */
  int8_t isbd_get_mt_txt( char *__mt_buff, size_t mt_buff_len );

  /**
   * @brief 
   * 
   * @param session 
   * @return int8_t 
   */
  int8_t isbd_init_session( isbd_session_ext_t *session );

  /**
   * @brief 
   * 
   * @param session 
   * @return int8_t 
   */
  int8_t isbd_clear_buffer( isbd_clear_buffer_t buffer );

  /**
   * @brief Execution command returns the received signal strength indication <rssi> from the 9602. Response is in
the form:
   * 
   * @param signal 
   * @return int8_t 
   */
  int8_t isbd_get_sig_q( uint8_t *signal );

  int8_t isbd_set_evt_report( isbd_evt_report_t *evt_report );

#endif