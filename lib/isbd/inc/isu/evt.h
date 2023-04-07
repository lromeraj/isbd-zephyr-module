#ifndef ISBD_EVT_H_
  #define ISBD_EVT_H_

  #include <stdint.h>

  #include "dte.h"

  typedef enum isbd_evt_name {

    ISBD_EVENT_UNK,

    /**
     * @brief Ring alert
    */
    ISBD_EVENT_RING,

    /**
     * @brief Signal quality
    */
    ISBD_EVENT_SIGQ,

    /**
     * @brief Service availability
     */
    ISBD_EVENT_SVCA,

    /**
     * @brief Automatic registration
     */
    ISBD_EVENT_AREG,

  } isbd_evt_name_t;

  typedef struct isbd_evt {
    
    isbd_evt_name_t name;
    
    union { 

      uint8_t sigq; // signal quality (strength)
      uint8_t svca; // service availability

      struct {
        uint8_t evt; 
        uint8_t err; 
      } areg;

    } data;

  } isbd_evt_t;

  /**
   * @brief Waits for any event triggered over DTE by the ISU 
   * 
   * @param isbd Iridium SBD instance
   * @param event A pointer where the resulting event should be stored
   * @param timeout_ms Maximum time to wait for 
   * @return isbd_err_t
   */
  isu_dte_err_t isu_evt_wait( isu_dte_t *isbd, isbd_evt_t *event, uint32_t timeout_ms );

#endif