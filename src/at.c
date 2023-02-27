#include "at.h"

#define AT_STR              "at"
#define AT_EOL              "\r"

// Basic commands chars
#define AT_CMD_H__          AT_STR

// Extended commands chars
#define AT_CMD_EXT_CHAR     "+"
#define AT_CMD_EXT_Q_CHAR   "?"
#define AT_CMD_EXT_EQ_CHAR  "="

#define AT_CMD_EXT_H__        AT_STR AT_CMD_EXT_CHAR
#define __AT_CMD_EXT_READ     AT_CMD_EXT_Q_CHAR AT_EOL
#define __AT_CMD_EXT_TEST     AT_CMD_EXT_EQ_CHAR AT_CMD_EXT_Q_CHAR AT_EOL

uint8_t at_cmd_p( char *at_buf, const char *cmd_name, uint8_t param ) {
  return sprintf( at_buf, AT_CMD_H__ "%s%hhu" AT_EOL, cmd_name, param );
}

uint8_t at_cmd( char *at_buf, const char *cmd_name ) {
  return sprintf( at_buf, AT_CMD_H__ "%s" AT_EOL, cmd_name );
}

uint8_t at_cmd_ext_t( char *at_buf, const char *cmd_name ) {
  return sprintf( at_buf, AT_CMD_EXT_H__ "%s" __AT_CMD_EXT_TEST, cmd_name );
}

uint8_t at_cmd_ext_r( char *at_buf, const char *cmd_name ) {
  return sprintf( at_buf, AT_CMD_EXT_H__ "%s" __AT_CMD_EXT_READ, cmd_name );
}

uint8_t at_cmd_ext_s( char *at_buf, const char *cmd_name, const char *params ) {
  return sprintf( at_buf, AT_CMD_EXT_H__ "%s" AT_CMD_EXT_EQ_CHAR "%s" AT_EOL, cmd_name, params );
}

uint8_t at_cmd_ext_p( char *at_buf, const char *cmd_name, uint8_t param ) {
  return sprintf( at_buf, AT_CMD_EXT_H__ "%s%hhu" AT_EOL, cmd_name, param );
}

uint8_t at_cmd_ext( char *at_buf, const char *cmd_name ) {
  return sprintf( at_buf, AT_CMD_EXT_H__ "%s" AT_EOL, cmd_name );
}