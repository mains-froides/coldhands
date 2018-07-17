#include <assert.h>
#include <string.h>

#include "strip.h"

#define MAGIC 0xf8319fa0

#define CHECK_MAGIC(strip) assert(strip->magic == MAGIC)

void strip_init(strip_t *strip, uint32_t spi_bus, uint8_t speed_mhz, uint8_t ci, uint8_t di, bool leds_invert, size_t leds_count, int dma_chan) {
  assert(leds_count <= CONFIG_MAX_LEDS_COUNT);

  strip->spi_bus_config.miso_io_num = -1;
  strip->spi_bus_config.mosi_io_num = di;
  strip->spi_bus_config.sclk_io_num = ci;
  strip->spi_bus_config.quadwp_io_num = -1;
  strip->spi_bus_config.quadhd_io_num = -1;
  strip->spi_bus_config.max_transfer_sz = 32 * (leds_count + 2);
  esp_err_t ret = spi_bus_initialize(spi_bus, &strip->spi_bus_config, dma_chan);
  ESP_ERROR_CHECK(ret);

  strip->spi_device_interface_config.clock_speed_hz = speed_mhz * 1000 * 1000;
  strip->spi_device_interface_config.mode = leds_invert ? 2 : 0;
  strip->spi_device_interface_config.spics_io_num = -1;
  strip->spi_device_interface_config.queue_size = 1;
  ret = spi_bus_add_device(spi_bus, &strip->spi_device_interface_config, &strip->spi);
  ESP_ERROR_CHECK(ret);

  strip->magic = MAGIC;
  strip->leds_count = leds_count;
  strip->leds_invert = leds_invert;
  memset(strip->leds_buffer, leds_invert ? ~0 : 0, 4 * (leds_count + 2));
  strip->leds_buffer[leds_count + 1] = leds_invert ? 0 : ~0;
}

void strip_set_led(strip_t *strip, size_t n, uint32_t color) {
  CHECK_MAGIC(strip);
  assert(n < strip->leds_count);
  strip->leds_buffer[n + 1] = strip->leds_invert ? ~color : color;
}

void strip_set_all_leds(strip_t *strip, uint32_t color) {
  CHECK_MAGIC(strip);
  if (strip->leds_invert)
    color = ~color;
  uint32_t *leds_buffer = strip->leds_buffer;
  for (size_t i = strip->leds_count; i > 0; i--)
    leds_buffer[i] = color;
}

void strip_transmit(strip_t *strip) {
  CHECK_MAGIC(strip);
  memset(&strip->trans, 0, sizeof(spi_transaction_t));
  strip->trans.length = 32 * (strip->leds_count + 2);
  strip->trans.tx_buffer = strip->leds_buffer;
  esp_err_t err = spi_device_transmit(strip->spi, &strip->trans);
  ESP_ERROR_CHECK(err);
}

void strip_queue_transaction(strip_t *strip, TickType_t ticks_to_wait) {
  CHECK_MAGIC(strip);
  memset(&strip->trans, 0, sizeof(spi_transaction_t));
  strip->trans.length = 32 * (strip->leds_count + 2);
  strip->trans.tx_buffer = strip->leds_buffer;
  esp_err_t err = spi_device_queue_trans(strip->spi, &strip->trans, ticks_to_wait);
  ESP_ERROR_CHECK(err);
}

void strip_get_transaction_result(strip_t *strip, TickType_t ticks_to_wait) {
  spi_transaction_t *t;
  esp_err_t err = spi_device_get_trans_result(strip->spi, &t, ticks_to_wait);
  ESP_ERROR_CHECK(err);
}

uint32_t hsv_to_color(uint16_t h, uint8_t s, uint8_t v, uint8_t global) {
  uint32_t r = 0xe0 | global;
  uint8_t *p = (uint8_t *) &r;
  fast_hsv2rgb_32bit(h, s, v, p+3, p+2, p+1);
  return r;
}
