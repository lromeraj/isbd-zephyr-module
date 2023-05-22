/**
 * @file dte.h
 * @author Javier Romera Llave (lromerajdev@gmail.com)
 * @brief Header definitions for Iridium SBD Data Terminal Equipment
 * @version 0.1
 * @date 2023-04-07
 * 
 * @copyright Copyright (c) 2023
 */
#ifndef ISBD_DTE_H_
  #define ISBD_DTE_H_

  #include "at_uart.h"

  typedef enum isu_dte_err {
    ISU_DTE_OK,
    ISU_DTE_ERR_AT,
    ISU_DTE_ERR_UNK,
    ISU_DTE_ERR_CMD,
    ISU_DTE_ERR_SETUP,
  } isu_dte_err_t;

  typedef struct isu_dte_config {
    struct at_uart_config at_uart;
  } isu_dte_config_t;

  typedef struct isu_dte {
    int err;
    at_uart_t at_uart;
    isu_dte_config_t config;
  } isu_dte_t;

  /**
   * @brief Retrieve last reported error. 
   * This function can be used to obtain a more detailed error
   * when any function from this library returns an error like <b>isbd_err_t</b>
   * 
   * @return int Last reported error
   */
  int isu_dte_get_err( isu_dte_t *dte );

  isu_dte_err_t isu_dte_setup( isu_dte_t *dte, struct isu_dte_config *config );
  isu_dte_err_t isu_dte_send_cmd( isu_dte_t *dte, const char *at_cmd_tmpl, ... );
  isu_dte_err_t isu_dte_send_tiny_cmd( isu_dte_t *dte, const char *at_cmd_tmpl, ... );

#endif