#include "at.h"

#define AT_STR              "at"
#define AT_EOL              "\r\n"

// Basic commands chars
#define AT_CMD_H__          AT_STR

// Extended commands chars
#define AT_CMD_Q_CHAR   "?"
#define AT_CMD_EQ_CHAR  "="

#define __AT_CMD_READ     AT_CMD_Q_CHAR AT_EOL
#define __AT_CMD_TEST     AT_CMD_EQ_CHAR AT_CMD_EQ_CHAR AT_EOL

uint8_t at_cmd_p( char *at_buf, const char *cmd_name, uint8_t param ) {
  return sprintf( at_buf, AT_CMD_H__ "%s%hhu" AT_EOL, cmd_name, param );
}

uint8_t at_cmd_e( char *at_buf, const char *cmd_name ) {
  return sprintf( at_buf, AT_CMD_H__ "%s" AT_EOL, cmd_name );
}

// test
uint8_t at_cmd_t( char *at_buf, const char *cmd_name ) {
  return sprintf( at_buf, AT_CMD_H__ "%s" __AT_CMD_TEST, cmd_name );
}

// read
uint8_t at_cmd_r( char *at_buf, const char *cmd_name ) {
  return sprintf( at_buf, AT_CMD_H__ "%s" __AT_CMD_READ, cmd_name );
}

// set
uint8_t at_cmd_s( char *at_buf, const char *cmd_name, const char *params ) {
  return sprintf( at_buf, AT_CMD_H__ "%s" AT_CMD_EQ_CHAR "%s" AT_EOL, cmd_name, params );
}