#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_struct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strip.h"

#ifndef CONFIG_HEART_SPI_INVERT
#define CONFIG_HEART_SPI_INVERT 0
#endif // CONFIG_HEART_SPI_INVERT

#ifndef CONFIG_HAND_SPI_INVERT
#define CONFIG_HAND_SPI_INVERT 0
#endif // CONFIG_HAND_SPI_INVERT

static strip_t hand;
static strip_t heart;

void app_main() {
  // Initialize the strips.
  strip_init(&hand, VSPI_HOST, CONFIG_HAND_SPI_MHZ, CONFIG_HAND_SPI_SCLK, CONFIG_HAND_SPI_MOSI,
      CONFIG_HAND_SPI_INVERT, CONFIG_HAND_LEDS_COUNT, 1);
  strip_init(&heart, HSPI_HOST, CONFIG_HEART_SPI_MHZ, CONFIG_HEART_SPI_SCLK, CONFIG_HEART_SPI_MOSI,
      CONFIG_HEART_SPI_INVERT, CONFIG_HEART_LEDS_COUNT, 2);
  // Turn strips off.
  strip_set_all_leds(&hand, COLOR_OFF);
  strip_transmit(&hand);
  strip_set_all_leds(&heart, COLOR_OFF);
  strip_transmit(&heart);
  // Test some colors 60° apart.
  for (uint16_t i = 0; i < 6 * 256; i += 256) {
    ESP_LOGD("COLOR", "hue = %d", i * 60 / 256);
    strip_set_all_leds(&heart, hsv_to_color(i, 255, 255, 31));
    strip_transmit(&heart);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  // Dummy animation.
  for (;;) {
    for (uint16_t i = 0; i < 6 * 256; i++) {
      strip_set_all_leds(&heart, hsv_to_color(i, 255, 255, 31));
      strip_transmit(&heart);
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    for (uint16_t i = 0; i < 256; i++) {
      strip_set_all_leds(&heart, hsv_to_color(0, i, 255, 31));
      strip_transmit(&heart);
      vTaskDelay(pdMS_TO_TICKS(20));
    }
    for (uint16_t i = 0; i < 256; i++) {
      strip_set_all_leds(&heart, hsv_to_color(0, 255, i, 31));
      strip_transmit(&heart);
      vTaskDelay(pdMS_TO_TICKS(20));
    }
    // Go between red and green
    for (uint8_t o = 0, i = 0; i < 6; o = ~o, i++) {
      strip_set_all_leds(&heart, rgb_to_color(o, ~o, 0, 31));
      strip_transmit(&heart);
      vTaskDelay(pdMS_TO_TICKS(500));
    }
    // Snake
    for (uint8_t c = 0; c < 6; c++) {
      strip_set_all_leds(&heart, COLOR_OFF);
      for (size_t n = 0; n < heart.leds_count; n++) {
        if (n > 0)
          strip_set_led(&heart, n-1, COLOR_OFF);
        strip_set_led(&heart, n, rgb_to_color(c % 3 == 0 ? 0xff : 0, c % 3 == 1 ? 0xff : 0,
                     c % 3 == 2 ? 0xff : 0, 31));
        strip_transmit(&heart);
        vTaskDelay(pdMS_TO_TICKS(50));
      }
    }
  }
}
