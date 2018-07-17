#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_struct.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strip.h"

// Current 100th of seconds is expected to be in "time".
#define SINUSOIDAL(period, min, max) \
  ((min) + ((max) - (min)) * (1 + sin(2 * M_PI * time / ((period) * 100))) / 2)

// Degrees to hue.
#define DEG_TO_HUE(d) (d * HSV_HUE_MAX / 360.0)

// Start and stop (in seconds) of the heart sequence. It is on before,
// and stays off after.
#define HAND_START 0
#define HAND_NONE 210

// Hand hue change (period is in seconds). Hue is between 0 and 360°, and
// the minimum value must be smaller than the maximum value (a modulo will
// be applied).
#define HAND_HUE_PERIOD 45
#define HAND_HUE_MIN 220
#define HAND_HUE_MAX 260

// Global intensity factor for leds (from 1 to 31).
#define HAND_GLOBAL 5

static void vHand(strip_t *strip, uint32_t time) {
  // The global envelope goes down linearly.
  float envelope;
#if HAND_START > 0
  if (time < HAND_START * 100)
    envelope = 1.0;
  else
#endif // HAND_START > 0
  if (time > HAND_NONE * 100)
    envelope = 0.0;
  else
    envelope = (HAND_NONE * 100.0 - time) / ((HAND_NONE - HAND_START) * 100);

  uint16_t hue = (uint16_t) round(SINUSOIDAL(HAND_HUE_PERIOD,
        DEG_TO_HUE(HAND_HUE_MIN), DEG_TO_HUE(HAND_HUE_MAX))) % HSV_HUE_MAX;

  if (time % 100 == 0) {
    ESP_LOGD("HAND", "time = %d, envelope = %g, value = %d, hue = %d", time / 100, envelope,
        (uint8_t) round(envelope * 255), hue);
  }

  strip_set_all_leds(strip, hsv_to_color(hue, 255, round(envelope * 255), HAND_GLOBAL));
}

// Start and stop (in seconds) of the heart sequence. It is off before,
// and stays on (intensity wide) after.
#define HEART_START 30
#define HEART_FULL 210

// Blink start time and period. The delay is in seconds, the period in hundredth
// of seconds. If the start time is 0, then no blinking will occur.
#define HEART_BLINK_START 230
#define HEART_BLINK_PERIOD 33

// Heart hue change (period is in seconds). Hue is between 0 and 360°, and
// the minimum value must be smaller than the maximum value (a modulo will
// be applied).
#define HEART_HUE_PERIOD 60
#define HEART_HUE_MIN 350
#define HEART_HUE_MAX 370

// Heart "lung" move (period is in seconds).
#define HEART_VALUE_PERIOD 6

// Global intensity factor for leds (from 1 to 31).
#define HEART_GLOBAL 5

static void vHeart(strip_t *strip, uint32_t time) {
  // The global envelope goes up slowly, with a square.
  float envelope;
#if HEART_START > 0
  if (time < HEART_START * 100)
    envelope = 0.0;
  else
#endif // HEART_START > 0
  if (time > HEART_FULL * 100)
    envelope = 1.0;
  else
    envelope = (time - HEART_START * 100.0) / ((HEART_FULL - HEART_START) * 100);
  envelope *= envelope;  // Let's go slowly

  uint16_t hue = (uint16_t) round(SINUSOIDAL(HEART_HUE_PERIOD,
        DEG_TO_HUE(HEART_HUE_MIN), DEG_TO_HUE(HEART_HUE_MAX))) % HSV_HUE_MAX;

  uint8_t value = round(SINUSOIDAL(HEART_VALUE_PERIOD, envelope / 2, envelope) * 255);

  if (time % 100 == 0) {
    ESP_LOGD("HEART", "time = %d, envelope = %g, value = %d, hue = %d", time / 100, envelope, value, hue);
  }

  strip_set_all_leds(strip, hsv_to_color(hue, 255, value, HEART_GLOBAL));
  if (HEART_BLINK_START && time >= HEART_BLINK_START * 100) {
    for (size_t i = (time / HEART_BLINK_PERIOD) % 3; i < strip->leds_count; i += 3)
      strip_set_led(strip, i, COLOR_OFF); 
  }
}

#ifndef CONFIG_HEART_SPI_INVERT
#define CONFIG_HEART_SPI_INVERT 0
#endif // CONFIG_HEART_SPI_INVERT

#ifndef CONFIG_HAND_SPI_INVERT
#define CONFIG_HAND_SPI_INVERT 0
#endif // CONFIG_HAND_SPI_INVERT

static strip_t hand_strip, heart_strip;

void app_main() {
  // Initialize strips and turn leds off.
  strip_init(&hand_strip, VSPI_HOST, CONFIG_HAND_SPI_MHZ, CONFIG_HAND_SPI_SCLK, CONFIG_HAND_SPI_MOSI,
      CONFIG_HAND_SPI_INVERT, CONFIG_HAND_LEDS_COUNT, 1);
  strip_set_all_leds(&hand_strip, COLOR_OFF);
  strip_transmit(&hand_strip);
  strip_init(&heart_strip, HSPI_HOST, CONFIG_HEART_SPI_MHZ, CONFIG_HEART_SPI_SCLK, CONFIG_HEART_SPI_MOSI,
      CONFIG_HEART_SPI_INVERT, CONFIG_HEART_LEDS_COUNT, 2);
  strip_set_all_leds(&heart_strip, COLOR_OFF);
  strip_transmit(&heart_strip);

  // Indefinitely schedule functions every 100th of seconds.
  TickType_t wakeup_time = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&wakeup_time, 1);
    vHand(&hand_strip, wakeup_time);
    // Send hand strip since it's ready, we will wait for completion once everything
    // else has been done.
    strip_queue_transaction(&hand_strip, portMAX_DELAY);
    vHeart(&heart_strip, wakeup_time);
    strip_transmit(&heart_strip);
    strip_get_transaction_result(&hand_strip, portMAX_DELAY);
  }
}
