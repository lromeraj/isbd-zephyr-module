#ifndef ISBD_EVT_H_
  #define ISBD_EVT_H_

  #include <stdint.h>

  #include "../isu/dte.h"

  typedef enum isbd_dte_evt_id {

    ISBD_DTE_EVT_UNK,

    /**
     * @brief Ring alert
    */
    ISBD_DTE_EVT_RING,

    /**
     * @brief Signal quality
    */
    ISBD_DTE_EVT_SIGQ,

    /**
     * @brief Service availability
     */
    ISBD_DTE_EVT_SVCA,

    /**
     * @brief Automatic registration
     */
    ISBD_DTE_EVT_AREG,

  } isbd_dte_evt_id_t;

  typedef struct isbd_dte_evt {
    
    isbd_dte_evt_id_t id;
    
    union { 

      uint8_t sigq; // signal quality (strength)
      uint8_t svca; // service availability

      struct {
        uint8_t evt; 
        uint8_t err; 
      } areg;

    };

  } isbd_dte_evt_t; // TODO: rename this again to isu_dte_evt_t :)

  /**
   * @brief Waits for any event triggered over DTE by the ISU 
   * 
   * @param dte ISU Data Terminal Equipment
   * @param event A pointer where the resulting event should be stored
   * @param timeout_ms Maximum time to wait for 
   * @return isbd_err_t
   */
  isu_dte_err_t isbd_dte_evt_wait( isu_dte_t *dte, isbd_dte_evt_t *event, uint32_t timeout_ms );

#endif