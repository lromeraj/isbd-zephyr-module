#ifndef ISBD_H_
  #define ISBD_H_

  #include <stdint.h>

  struct isbd_config {
    bool verbose; /** Use verbose mode */
    struct device *dev;
  };

  typedef enum isbd_err {
    ISBD_OK     = 0,
    ISBD_ERR,
  } isbd_err_t;

  isbd_err_t isbd_setup( struct isbd_config *config );

  isbd_err_t isbd_test_at();
  isbd_err_t isbd_fetch_rtc( char *__rtc );
  isbd_err_t isbd_fetch_imei( char *__imei );
  isbd_err_t isbd_fetch_revision( char *__revision );

  isbd_err_t isbd_enable_flow_control( bool enable );

  isbd_err_t isbd_set_mo_txt( const char *txt );
  isbd_err_t isbd_set_mo_txt_l( const char *txt );

  isbd_err_t isbd_mo_to_mt( char *__out );

  isbd_err_t isbd_get_mt_txt( char *__mt_buff );

#endif