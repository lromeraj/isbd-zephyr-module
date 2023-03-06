#include "at.h"

#define AT_STR           "at"
#define AT_CMD_Q_STR     "?"
#define AT_CMD_EQ_STR    "="

// AT command prefix
#define AT_CMD_H          AT_STR

// AT command read suffix
#define AT_CMD_READ       AT_CMD_Q_STR AT_CMD_EOL

// AT command test suffix
#define AT_CMD_TEST       AT_CMD_EQ_STR AT_CMD_Q_STR AT_CMD_EOL

// AT command end of line
#define AT_CMD_EOL        "\r"


/**
 * @brief This macro is used to safely write into
 * a predefined AT buffer. Returns the number of bytes written into the buffer.
 */
#define BUILD_AT_CMD_RET( buff, ... ) \
  uint8_t _M_written = snprintf( buff, AT_MAX_CMD_SIZE, __VA_ARGS__ ); \
  if ( _M_written < AT_MAX_CMD_SIZE ) { \
    return _M_written; \
  } else { \
    return AT_MAX_CMD_SIZE-1; \
  }

// exec
uint8_t at_cmd_e( char *at_buf, const char *cmd_name ) {
  BUILD_AT_CMD_RET( 
    at_buf, AT_CMD_H "%s" AT_CMD_EOL, cmd_name );
}

// exec with param
uint8_t at_cmd_p( char *at_buf, const char *cmd_name, uint8_t param ) {
  BUILD_AT_CMD_RET( 
    at_buf, AT_CMD_H "%s%hhu" AT_CMD_EOL, cmd_name, param );
}

// test
uint8_t at_cmd_t( char *at_buf, const char *cmd_name ) {
  BUILD_AT_CMD_RET(
    at_buf, AT_CMD_H "%s" AT_CMD_TEST, cmd_name );
}

// read
uint8_t at_cmd_r( char *at_buf, const char *cmd_name ) {
  BUILD_AT_CMD_RET( 
    at_buf, AT_CMD_H "%s" AT_CMD_READ, cmd_name );
}

// set
uint8_t at_cmd_s( char *at_buf, const char *cmd_name, const char *params ) {
  BUILD_AT_CMD_RET(
    at_buf, AT_CMD_H "%s" AT_CMD_EQ_STR "%s" AT_CMD_EOL, cmd_name, params );
}