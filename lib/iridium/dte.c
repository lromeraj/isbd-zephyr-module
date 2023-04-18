#include "inc/isu/dte.h"

int isu_dte_get_err( isu_dte_t *isbd ) {
  return isbd->err;
}

isu_dte_err_t isu_dte_setup( isu_dte_t *isbd, isu_dte_config_t *isu_dte_config ) {

  isbd->config = *isu_dte_config;

  at_uart_err_t ret = at_uart_setup( 
    &isbd->at_uart, &isu_dte_config->at_uart );

  return ret == AT_UART_OK ? ISU_DTE_OK : ISU_DTE_ERR_SETUP;
}

isu_dte_err_t isu_dte_send_tiny_cmd( isu_dte_t *isbd, const char *at_cmd_tmpl, ... ) {

  va_list args;
  va_start( args, at_cmd_tmpl );

  AT_DEFINE_CMD_BUFF( at_buf );
  
  at_uart_err_t err = at_uart_send_vcmd( 
    &isbd->at_uart, at_buf, sizeof( at_buf ), at_cmd_tmpl, args );

  va_end( args );

  isbd->err = err;

  return err == AT_UART_OK ? ISU_DTE_OK : ISU_DTE_ERR_AT;
}

isu_dte_err_t isu_dte_send_cmd( isu_dte_t *isbd, const char *at_cmd_tmpl, ... ) { 
  return ISU_DTE_ERR_CMD; 
}


