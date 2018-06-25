#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_struct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CONFIG_LEDS_INVERT
#define CONFIG_LEDS_INVERT 0
#endif // CONFIG_LEDS_INVERT

static uint32_t leds_buffer[CONFIG_LEDS_COUNT + 2];

static void leds_init() {
  leds_buffer[0] = CONFIG_LEDS_INVERT ? ~0 : 0;
  leds_buffer[CONFIG_LEDS_COUNT + 1] = CONFIG_LEDS_INVERT ? 0 : ~0;
}

static uint32_t leds_to_word(uint8_t r, uint8_t g, uint8_t b, uint8_t global) {
  return 0xe0 | global | (b << 8) | (g << 16) | (r << 24);
}

static void leds_set_uniform(uint8_t r, uint8_t g, uint8_t b, uint8_t global) {
  assert(global <= 31);
  uint32_t word = leds_to_word(r, g, b, global);
  for (int i = 1; i < CONFIG_LEDS_COUNT + 1; i++)
    leds_buffer[i] = word;
}

static void leds_set_led(size_t n, uint8_t r, uint8_t g, uint8_t b,
                         uint8_t global) {
  assert(n <= CONFIG_LEDS_COUNT);
  leds_buffer[n + 1] = leds_to_word(r, g, b, global);
}

static void leds_transmit(spi_device_handle_t spi) {
  spi_transaction_t t;
  memset(&t, 0, sizeof t);
  t.length = 8 * sizeof leds_buffer;
  t.tx_buffer = leds_buffer;
  esp_err_t err = spi_device_transmit(spi, &t);
  ESP_ERROR_CHECK(err);
}

void app_main() {
  esp_err_t ret;
  spi_device_handle_t spi;
  spi_bus_config_t buscfg = {
      .miso_io_num = -1,
      .mosi_io_num = CONFIG_LEDS_SPI_MOSI,
      .sclk_io_num = CONFIG_LEDS_SPI_SCLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 8 * sizeof leds_buffer,
  };
  spi_device_interface_config_t devcfg = {
      .clock_speed_hz = CONFIG_LEDS_SPI_MHZ * 1000 * 1000,
      .mode =
          CONFIG_LEDS_INVERT ? 2 : 0, // SPI mode 0 or 2 depending on inversion
      .spics_io_num = -1,             // No CS pin
      .queue_size = 1,
  };
  // Initialize the SPI bus
  ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
  ESP_ERROR_CHECK(ret);
  // Attach the leds to the SPI bus
  ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
  ESP_ERROR_CHECK(ret);
  // Initialize the leds buffer
  leds_init();
  for (;;) {
    // Blue is the warmest color
    leds_set_uniform(0, 0, 255, 10);
    leds_transmit(spi);
    vTaskDelay(pdMS_TO_TICKS(1000));
    // Go between red and green
    for (uint8_t o = 0, i = 0; i < 6; o = ~o, i++) {
      leds_set_uniform(o, ~o, 0, 10);
      leds_transmit(spi);
      vTaskDelay(pdMS_TO_TICKS(500));
    }
    // Snake
    for (uint8_t c = 0; c < 6; c++) {
      leds_set_uniform(0, 0, 0, 0);
      for (size_t n = 0; n < CONFIG_LEDS_COUNT; n++) {
        if (n > 0)
          leds_set_led(n - 1, 0, 0, 0, 0);
        leds_set_led(n, c % 3 == 0 ? 0xff : 0, c % 3 == 1 ? 0xff : 0,
                     c % 3 == 2 ? 0xff : 0, 10);
        leds_transmit(spi);
        vTaskDelay(pdMS_TO_TICKS(50));
      }
    }
  }
}
