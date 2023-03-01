#ifndef AT_H_
#define AT_H_

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define AT_NO_PARAM     ((uint8_t)(-1))

#define __AT_BUFF( name ) \
  unsigned char name[256] = ""

uint8_t at_cmd_e( char *at_buf, const char *cmd_name );
uint8_t at_cmd_p( char *at_buf, const char *cmd_name, uint8_t param );
uint8_t at_cmd_t( char *at_buf, const char *cmd_name );
uint8_t at_cmd_r( char *at_buf, const char *cmd_name );
uint8_t at_cmd_s( char *at_buf, const char *cmd_name, const char *params );

#endif