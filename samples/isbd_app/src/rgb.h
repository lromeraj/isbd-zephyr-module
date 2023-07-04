#ifndef RGB_H_
  #define RGB_H_

  #include <zephyr/kernel.h>
  #include <zephyr/drivers/led_strip.h>
  #include <zephyr/device.h>
  #include <zephyr/drivers/spi.h>
  #include <zephyr/sys/util.h>

  void rgb_setup();
  void rgb_show();



#endif