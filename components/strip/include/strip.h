#ifndef STRIP_H
#define STRIP_H

#include <stdint.h>
#include "driver/spi_master.h"
#include "fast_hsv2rgb.h" // Reexport

#define MAX_LEDS_COUNT 50

typedef struct {
  uint32_t magic;
  spi_bus_config_t spi_bus_config;
  spi_device_interface_config_t spi_device_interface_config;
  spi_device_handle_t spi;
  spi_transaction_t trans;
  size_t leds_count;
  bool leds_invert;
  uint32_t leds_buffer[MAX_LEDS_COUNT + 2];
} strip_t;

void strip_init(strip_t *strip, uint32_t spi_bus, uint8_t speed_mhz, uint8_t ci, uint8_t di, bool leds_invert, size_t leds_count, int dma_chan);
void strip_set_led(strip_t *strip, size_t n, uint32_t color);
void strip_set_all_leds(strip_t *strip, uint32_t color);
void strip_transmit(strip_t *strip);
void strip_queue_transaction(strip_t *strip, TickType_t ticks_to_wait);
void strip_get_transaction_result(strip_t *strip, TickType_t ticks_to_wait);

static inline
uint32_t rgb_to_color(uint8_t r, uint8_t g, uint8_t b, uint8_t global) {
  return 0xe0 | global | (b << 8) | (g << 16) | (r << 24);
}

uint32_t hsv_to_color(uint16_t h, uint8_t s, uint8_t v, uint8_t global);

#define COLOR_OFF rgb_to_color(0, 0, 0, 0)

#define GLOBAL_MAX 31

#endif // STRIP_H
