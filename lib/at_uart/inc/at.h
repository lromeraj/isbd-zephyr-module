#ifndef AT_H_
  #define AT_H_

  #include <stdio.h>
  #include <string.h>
  #include <stdint.h>

  #define AT_MAX_CMD_LEN    255
  #define AT_MAX_CMD_SIZE   (AT_MAX_CMD_LEN + 1)

  #define AT_DEFINE_CMD_BUFF( name ) \
    char name[ AT_MAX_CMD_SIZE ];

  #define AT_STR           "at"
  #define AT_CMD_EOL_STR    "\r"

  #define GEN_AT_CMD_TMPL( tmpl ) \
    AT_STR "%s" tmpl AT_CMD_EOL_STR

  #define AT_CMD_TMPL_EXEC      GEN_AT_CMD_TMPL( "" )
  #define AT_CMD_TMPL_EXEC_INT  GEN_AT_CMD_TMPL( "%u" )
  #define AT_CMD_TMPL_EXEC_STR  GEN_AT_CMD_TMPL( "%s" )
  #define AT_CMD_TMPL_TEST      GEN_AT_CMD_TMPL( "=?" )
  #define AT_CMD_TMPL_READ      GEN_AT_CMD_TMPL( "?" )
  #define AT_CMD_TMPL_SET_INT   GEN_AT_CMD_TMPL( "=%d" )
  #define AT_CMD_TMPL_SET_STR   GEN_AT_CMD_TMPL( "=%s" )

#endif