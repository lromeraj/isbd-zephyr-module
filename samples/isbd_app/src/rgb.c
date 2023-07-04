#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER( rgb );

#include "rgb.h"

#define STRIP_NODE		DT_ALIAS(led_strip)
#define STRIP_NUM_PIXELS	DT_PROP(DT_ALIAS(led_strip), chain_length)

#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);

#define RGB_BLACK   RGB(0x00, 0x00, 0x00)
#define RGB_RED     RGB(0xff, 0x00, 0x00)
#define RGB_GREEN   RGB(0x00, 0xff, 0x00)
#define RGB_BLUE    RGB(0x00, 0x00, 0xff)
#define RGB_WHITE   RGB(0xff, 0xff, 0xff)

static const struct led_rgb colors[] = {
	RGB(0x00, 0xff, 0x00), /* green */
	RGB(0xff, 0x00, 0x00), /* red */
	RGB(0x00, 0x00, 0xff), /* blue */
};

struct led_rgb pixels[ STRIP_NUM_PIXELS ] = {};

void rgb_setup() {
  if ( device_is_ready( strip ) ) {
		LOG_INF( "Found LED strip device %s", strip->name );
	} else {
		LOG_ERR( "LED strip device %s is not ready", strip->name );
	}
}

void rgb_show() {
//   // memset(&pixels, 0x00, sizeof(pixels));
// 	// memcpy(&pixels[cursor], &colors[color], sizeof(struct led_rgb));
  size_t cursor = 0, color = 0;
	int rc;

  while (1) {

		memset(&pixels, 0x00, sizeof(pixels));
		memcpy(&pixels[cursor], &colors[color], sizeof(struct led_rgb));
    
		rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);

		if (rc) {
			LOG_ERR("couldn't update strip: %d", rc);
		}

		cursor++;
		if (cursor >= STRIP_NUM_PIXELS) {
			cursor = 0;
			color++;
			if (color == ARRAY_SIZE(colors)) {
				color = 0;
			}
		}

		k_sleep( K_MSEC( 500 ) );
	}

}