#include "buttons.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "board.h"
#include "events.h"

#define DEBOUNCE_US 50000 //ignore edges within 50 ms of  the last 

typedef struct { 
  int pin;
  evt_type_t evt;
  int64_t last_us;
}button_t;

static button_t s_buttons[] = { 
  {.pin = PIN_BTN_POUR, .evt = EVT_BUTTON_POUR},
};

//RUns in interruptr context: no logging, no blocking, no decisions
//It timestamps the press, drops contact bounch, and posts an event. 
static void IRAM_ATTR button_isr(void *arg) { 
  button_t *b = (button_t *)arg;
  int64_t now = esp_timer_get_time();
  if (now -b->last_us < DEBOUNCE_US) { 
    return;
  }
  b->last_us = now; 

  BaseType_t woken = pdFALSE;
  event_t e = { .type = b->evt, .t_us = now };
  events_post_from_isr(&e, &woken);
  if (woken) { 
    portYIELD_FROM_ISR();
  }
}

void buttons_init(void) { 
  const gpio_config_t cfg = { 
    .pin_bit_mask = (1ULL << PIN_BTN_POUR),
    .mode = GPIO_MODE_INPUT, 
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_NEGEDGE, //pressed = pull to GNG 
  };
  ESP_ERROR_CHECK(gpio_config(&cfg));

  //One shared GPIO Interrupt service for whole project 
  //THE HC -SR04 driver will add its own handler later 
  ESP_ERROR_CHECK(gpio_install_isr_service(0));

  for (size_t i = 0; i < sizeof s_buttons / sizeof s_buttons[0]; i++) {
    ESP_ERROR_CHECK(gpio_isr_handler_add(s_buttons[i].pin, button_isr, &s_buttons[i]));
  }
}
