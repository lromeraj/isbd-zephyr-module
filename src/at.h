#ifndef AT_H_
#define AT_H_

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define AT_NO_PARAM     ((uint8_t)(-1))

#define __AT_BUFF( name ) \
  unsigned char name[256] = ""

/**
 * @brief Used to build AT exec commands. Use this when you need
 * to execute a command without params and extra suffixes
 * 
 * @param at_buf Output buffer, should have at least 141 bytes of space. Use __AT_BUFF macro whenever possible.
 * @param cmd_name AT command name
 * @return uint8_t bytes written to the given buffer
 */
uint8_t at_cmd_e( char *at_buf, const char *cmd_name );

/**
 * @brief 
 * 
 * @param at_buf 
 * @param cmd_name 
 * @param param 
 * @return uint8_t 
 */
uint8_t at_cmd_p( char *at_buf, const char *cmd_name, uint8_t param );

/**
 * @brief 
 * 
 * @param at_buf 
 * @param cmd_name 
 * @return uint8_t 
 */
uint8_t at_cmd_t( char *at_buf, const char *cmd_name );

/**
 * @brief 
 * 
 * @param at_buf 
 * @param cmd_name 
 * @return uint8_t 
 */
uint8_t at_cmd_r( char *at_buf, const char *cmd_name );

/**
 * @brief 
 * 
 * @param at_buf 
 * @param cmd_name 
 * @param params 
 * @return uint8_t 
 */
uint8_t at_cmd_s( char *at_buf, const char *cmd_name, const char *params );

#endif