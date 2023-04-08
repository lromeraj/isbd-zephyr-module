#ifndef ISU_H_
  #define ISU_H_

  #include "isu/dte.h"

  typedef enum isu_clear_buffer {

    /**
     * @brief  Clear the mobile originated buffer
     */
    ISU_CLEAR_MO_BUFF      = 0,

    /**
     * @brief Clear the mobile terminated buffer
     */
    ISU_CLEAR_MT_BUFF      = 1,

    /**
     * @brief Clear both the mobile originated and mobile terminated buffers
     */
    ISU_CLEAR_MO_MT_BUFF   = 2, 

  } isu_clear_buffer_t;

  typedef enum isu_ring_sts {

    /**
     * @brief  Clear the mobile originated buffer
     */
    ISU_RING_STS_NOT_RECV   = 0,

    /**
     * @brief Clear the mobile terminated buffer
     */
    ISU_RING_STS_RECV       = 1, 

  } isu_ring_sts_t;

  typedef enum isu_mt_alert {

    /**
     * @brief Enable SBD Ring Alert ring indication (default)
     */
    ISU_MT_ALERT_ENABLED     = 1,

    /**
     * @brief Disable SBD Ring Alert indication
     */
    ISU_MT_ALERT_DISABLED    = 0,
    
  } isu_mt_alert_t;

  typedef enum isu_net_reg_sts {

    /**
     * @brief Transceiver is detached as a result of a successful 
     * +SBDDET or +SBDI command.
     */
    ISU_NET_REG_STS_DETACHED         = 0,

    /**
     * @brief Transceiver is attached but has 
     * not provided a good location since it was last detached
     */
    ISU_NET_REG_STS_NOT_REGISTERED   = 1,

    /**
     * @brief Transceiver is attached with a good location. 
     * @note This may be the case even when the most recent 
     * attempt did not provide a good location
     */
    ISU_NET_REG_STS_REGISTERED       = 2,

    /**
     * @brief The gateway is denying service to the Transceiver
     */
    ISU_NET_REG_STS_DENIED           = 3,

  } isu_net_reg_sts_t;

  typedef struct isu_session_ext {
    uint8_t mo_sts, mt_sts;
    uint16_t mo_msn, mt_msn;
    uint16_t mt_len;
    uint8_t mt_queued;
  } isu_session_ext_t;

  typedef struct isu_evt_report {
    uint8_t mode;
    uint8_t signal;
    uint8_t service;
  } isu_evt_report_t;

  /**
   * @brief Query the device for Iridium system time if available
   * 
   * @param __rtc 
   * @return int8_t 
   */
  isu_dte_err_t isu_get_rtc( isu_dte_t *isbd, char *__rtc , size_t rtc_len );

  /**
   * @brief Query the device IMEI
   * 
   * @param __imei Resulting IMEI memory buffer
   * @return int8_t
   */          
  isu_dte_err_t isu_get_imei( isu_dte_t *dte, char *__imei, size_t imei_len );

  /**
   * @brief Query the device revision
   * 
   * @param __revision 
   * @return int8_t 
   */
  isu_dte_err_t isu_get_revision( isu_dte_t *dte, char *__rev, size_t rev_len );

  /**
   * @brief Transfer a SBD text message from the DTE 
   * to the single mobile originated buffer.
   * 
   * @param txt SBD message with a maximum length of 120 bytes (excluding null char)
   * @return dte_at_code_t 
   */
  isu_dte_err_t isu_set_mo_txt( isu_dte_t *dte, const char *txt );

  /**
   * @brief Transfer a longer SBD text message from the DTE 
   * to the single mobile originated buffer.
   * 
   * @param txt SBD message with a maximum length of 340 bytes (excluding null char)
   * @return dte_at_code_t 
   */
  // int8_t dte_set_mo_txt_l( char *txt );

  /**
   * @brief Transfer the contents of the mobile originated buffer 
   * to the mobile terminated buffer.
   * 
   * @param __out ISU string response. Use NULL to ignore the string response
   * @return int8_t 
   */
  isu_dte_err_t isu_mo_to_mt( isu_dte_t *dte, char *out, uint16_t out_len );

  /**
   * @brief 
   * 
   * @param msg 
   * @param msg_len 
   * @param csum 
   * @return dte_err_t 
   */
  isu_dte_err_t isu_get_mt( isu_dte_t *dte, uint8_t *msg, uint16_t *msg_len, uint16_t *csum );

  /**
   * @brief 
   * 
   * @param __msg 
   * @param msg_len 
   * @return int8_t 
   */
  isu_dte_err_t isu_set_mo( isu_dte_t *dte, const uint8_t *msg, uint16_t msg_len );

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
  isu_dte_err_t isu_get_mt_txt( isu_dte_t *dte, char *__mt_buff, size_t mt_buff_len );

  /**
   * @brief 
   * 
   * @param session 
   * @return int8_t 
   */
  isu_dte_err_t isu_init_session( isu_dte_t *dte, isu_session_ext_t *session, bool alert );

  /**
   * @brief 
   * 
   * @param session 
   * @return int8_t 
   */
  isu_dte_err_t isu_clear_buffer( isu_dte_t *dte, isu_clear_buffer_t buffer );

  /**
   * @brief Execution command returns the received signal strength indication <rssi> from the 9602.
   * 
   * @param signal 
   * @return int8_t 
   */
  isu_dte_err_t isu_get_sig_q( isu_dte_t *dte, uint8_t *signal );

  /**
   * @brief Set indicator event reporting
   * 
   * @param evt_report Struct containing reporting configuration
   * @return dte_err_t 
   */
  isu_dte_err_t isu_set_evt_report( isu_dte_t *dte, isu_evt_report_t *evt_report );

  /**
   * @brief Enable or disable the ISU to listen for SBD Ring Alerts
   *  
   * @param alert 
   * @return dte_err_t 
   */
  isu_dte_err_t isu_set_mt_alert( isu_dte_t *dte, isu_mt_alert_t alert );

  /**
   * @brief Query the current ring indication mode
   * 
   * @param alert 
   * @return dte_err_t 
   */
  isu_dte_err_t isu_get_mt_alert( isu_dte_t *dte, isu_mt_alert_t *alert );

  /**
   * @brief Triggers an SBD session to perform a manual SBD Network Registration
   * 
   * @param reg_sts A pointer where the resulting registration status will be stored 
   * @return dte_err_t 
   */
  isu_dte_err_t isu_net_reg( isu_dte_t *dte, isu_net_reg_sts_t *reg_sts );

  /**
   * @brief Query the ring indication status, returning the reason 
   * for the most recent assertion of the Ring Indicator
   * 
   * @param ring_sts
   * @return dte_err_t
   */
  isu_dte_err_t isu_get_ring_sts( isu_dte_t *dte, isu_ring_sts_t *ring_sts );

#endif