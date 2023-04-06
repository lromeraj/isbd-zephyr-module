#ifndef ISBD_H_
  #define ISBD_H_

  // TODO: this module should be renamed to ISBD_CMD or ISBD_AT
  #include <stdint.h>

  #include "at_uart.h"

  typedef enum isbd_err {
    ISBD_OK,
    ISBD_ERR_AT,
    ISBD_ERR_UNK,
    ISBD_ERR_CMD,
    ISBD_ERR_SETUP,
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


  typedef enum isbd_ring_sts {

    /**
     * @brief  Clear the mobile originated buffer
     */
    ISBD_RING_STS_NOT_RECV   = 0,

    /**
     * @brief Clear the mobile terminated buffer
     */
    ISBD_RING_STS_RECV       = 1, 

  } isbd_ring_sts_t;

  typedef enum isbd_mt_alert {

    /**
     * @brief Enable SBD Ring Alert ring indication (default)
     */
    ISBD_MT_ALERT_ENABLED     = 1,

    /**
     * @brief Disable SBD Ring Alert indication
     */
    ISBD_MT_ALERT_DISABLED    = 0,
    
  } isbd_mt_alert_t;

  typedef enum isbd_net_reg_sts {

    /**
     * @brief Transceiver is detached as a result of a successful 
     * +SBDDET or +SBDI command.
     */
    ISBD_NET_REG_STS_DETACHED         = 0,

    /**
     * @brief Transceiver is attached but has 
     * not provided a good location since it was last detached
     */
    ISBD_NET_REG_STS_NOT_REGISTERED   = 1,

    /**
     * @brief Transceiver is attached with a good location. 
     * @note This may be the case even when the most recent 
     * attempt did not provide a good location
     */
    ISBD_NET_REG_STS_REGISTERED       = 2,

    /**
     * @brief The gateway is denying service to the Transceiver
     */
    ISBD_NET_REG_STS_DENIED           = 3,

  } isbd_net_reg_sts_t;

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

  typedef struct isbd_config {
    struct at_uart_config at_uart;
  } isbd_config_t;

  typedef struct isbd {
    int err;
    struct at_uart at_uart;
    struct isbd_config config;
  } isbd_t;

  /**
   * @brief Retrieve last reported error. 
   * This function can be used to obtain a more detailed error
   * when any function from this library returns an error like <b>isbd_err_t</b>
   * 
   * @return int Last reported error
   */
  int isbd_get_err( isbd_t *isbd );
  
  /**
   * @brief Computes the checksum from the given message buffer
   * 
   * @param msg_buf Message buffer
   * @param msg_buf_len Message buffer length
   * @return uint16_t Message checksum
   */
  uint16_t isbd_compute_checksum( const uint8_t *msg_buf, uint16_t msg_buf_len );

  isbd_err_t isbd_setup( isbd_t *isbd, struct isbd_config *config );

  /**
   * @brief Query the device for Iridium system time if available
   * 
   * @param __rtc 
   * @return int8_t 
   */
  isbd_err_t isbd_get_rtc( isbd_t *isbd, char *__rtc , size_t rtc_len );

  /**
   * @brief Query the device IMEI
   * 
   * @param __imei Resulting IMEI memory buffer
   * @return int8_t
   */          
  isbd_err_t isbd_get_imei( isbd_t *isbd, char *__imei, size_t imei_len );

  /**
   * @brief Query the device revision
   * 
   * @param __revision 
   * @return int8_t 
   */
  isbd_err_t isbd_get_revision( isbd_t *isbd, char *__rev, size_t rev_len );

  /**
   * @brief Transfer a SBD text message from the DTE 
   * to the single mobile originated buffer.
   * 
   * @param txt SBD message with a maximum length of 120 bytes (excluding null char)
   * @return isbd_at_code_t 
   */
  isbd_err_t isbd_set_mo_txt( isbd_t *isbd, const char *txt );

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
  isbd_err_t isbd_mo_to_mt( isbd_t *isbd, char *out, uint16_t out_len );

  /**
   * @brief 
   * 
   * @param msg 
   * @param msg_len 
   * @param csum 
   * @return isbd_err_t 
   */
  isbd_err_t isbd_get_mt( isbd_t *isbd, uint8_t *msg, uint16_t *msg_len, uint16_t *csum );

  /**
   * @brief 
   * 
   * @param __msg 
   * @param msg_len 
   * @return int8_t 
   */
  isbd_err_t isbd_set_mo( isbd_t *isbd, const uint8_t *msg, uint16_t msg_len );

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
  isbd_err_t isbd_get_mt_txt( isbd_t *isbd, char *__mt_buff, size_t mt_buff_len );

  /**
   * @brief 
   * 
   * @param session 
   * @return int8_t 
   */
  isbd_err_t isbd_init_session( isbd_t *isbd, isbd_session_ext_t *session, bool alert );

  /**
   * @brief 
   * 
   * @param session 
   * @return int8_t 
   */
  isbd_err_t isbd_clear_buffer( isbd_t *isbd, isbd_clear_buffer_t buffer );

  /**
   * @brief Execution command returns the received signal strength indication <rssi> from the 9602.
   * 
   * @param signal 
   * @return int8_t 
   */
  isbd_err_t isbd_get_sig_q( isbd_t *isbd, uint8_t *signal );

  /**
   * @brief Set indicator event reporting
   * 
   * @param evt_report Struct containing reporting configuration
   * @return isbd_err_t 
   */
  isbd_err_t isbd_set_evt_report( isbd_t *isbd, isbd_evt_report_t *evt_report );

  /**
   * @brief Enable or disable the ISU to listen for SBD Ring Alerts
   *  
   * @param alert 
   * @return isbd_err_t 
   */
  isbd_err_t isbd_set_mt_alert( isbd_t *isbd, isbd_mt_alert_t alert );

  /**
   * @brief Query the current ring indication mode
   * 
   * @param alert 
   * @return isbd_err_t 
   */
  isbd_err_t isbd_get_mt_alert( isbd_t *isbd, isbd_mt_alert_t *alert );

  /**
   * @brief Triggers an SBD session to perform a manual SBD Network Registration
   * 
   * @param reg_sts A pointer where the resulting registration status will be stored 
   * @return isbd_err_t 
   */
  isbd_err_t isbd_net_reg( isbd_t *isbd, isbd_net_reg_sts_t *reg_sts );

  /**
   * @brief Query the ring indication status, returning the reason 
   * for the most recent assertion of the Ring Indicator
   * 
   * @param ring_sts
   * @return isbd_err_t
   */
  isbd_err_t isbd_get_ring_sts( isbd_t *isbd, isbd_ring_sts_t *ring_sts );
  
  // isbd_err_t isbd_wait_unblock();

#endif  