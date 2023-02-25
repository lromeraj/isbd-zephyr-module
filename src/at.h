#ifndef AT_H_
#define AT_H_

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define AT_NO_PARAM     ((uint8_t)(-1))

#define __AT_BUFF( name ) \
  unsigned char name[256] = ""

uint8_t at_cmd( char *at_buf, const char *cmd_name );
uint8_t at_cmd_p( char *at_buf, const char *cmd_name, uint8_t param );

uint8_t at_cmd_ext( char *at_buf, const char *cmd_name );
uint8_t at_cmd_ext_t( char *at_buf, const char *cmd_name );
uint8_t at_cmd_ext_r( char *at_buf, const char *cmd_name );
uint8_t at_cmd_ext_e( char *at_buf, const char *cmd_name, uint8_t param );

/**
 * @brief Builds an extended AT command to set a value
 * 
 * `at+<cmd_name>=<params>`
 * 
 * 
 * @param at_buf 
 * @param cmd_name 
 * @param params 
 * @return uint8_t 
 */
uint8_t at_cmd_ext_s( char *at_buf, const char *cmd_name, const char *params );

#endif