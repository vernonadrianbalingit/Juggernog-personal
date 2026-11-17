#include <driver/gpio.h>
#include <relay.h>
#include <board.h> 

#define RELAY_LEVEL_ON 1 //active high
#define RELAY_LEVEL_OFF 0


esp_err_t relay_init(void) { 
  gpio_set_level(PIN_RELAY, RELAY_LEVEL_OFF); //latch off before the pin drives
  //relay never sees a stray LOW while we setup the pin 
  const gpio_config_t cfg = { 
    .pin_bit_mask = 1ULL << PIN_RELAY,
    .mode =         GPIO_MODE_OUTPUT,
    .pull_up_en =   GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type =    GPIO_INTR_DISABLE,
  };
  esp_err_t err = gpio_config(&cfg);
  gpio_set_level(PIN_RELAY, RELAY_LEVEL_OFF);
  return err;
}

void relay_set(bool on) { 
  gpio_set_level(PIN_RELAY, on ? RELAY_LEVEL_ON : RELAY_LEVEL_OFF);
}