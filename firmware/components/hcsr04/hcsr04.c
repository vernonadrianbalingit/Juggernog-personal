#include "hcsr04.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "board.h"

#define ECHO_TIMEOUT_MS 40 // 400 cm round rtrip is 23 ms 
#define ECHO_MIN_US 116 // 2 cm 
#define ECHO_MAX_US 23200 //400 cm
#define US_PER_CM 58.0f // sound is -29 us per cm , there an back 

static TaskHandle_t s_waiter; //task currently waiting for an echo 
static volatile int64_t s_rise_us;

//Fires on both edges of Echo 
//Rising edge: remember the time 
//falling edge: send the pulse width straight to the waiting task 
static void IRAM_ATTR echo_isr(void *arg) { 
  int64_t now = esp_timer_get_time();
  if (gpio_get_level(PIN_ECHO)) { 
    s_rise_us = now;
  } else if (s_waiter != NULL) { 
    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(s_waiter, (uint32_t)(now-s_rise_us), eSetValueWithOverwrite, &woken);
    if (woken) { 
      portYIELD_FROM_ISR();
    }
  }
}

void hcsr04_init(void) { 
  const gpio_config_t trig = {
    .pin_bit_mask = 1ULL << PIN_TRIG,
    .mode = GPIO_MODE_OUTPUT,
  };
  ESP_ERROR_CHECK(gpio_config(&trig));
  gpio_set_level(PIN_TRIG, 0);

  const gpio_config_t echo = { 
    .pin_bit_mask = 1ULL << PIN_ECHO,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE, //divider sets the level
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_ANYEDGE,
  };
  ESP_ERROR_CHECK(gpio_config(&echo));
  ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_ECHO, echo_isr, NULL));
}

bool hcsr04_read_cm(float *cm) { 
  uint32_t width_us = 0;

  s_waiter = xTaskGetCurrentTaskHandle();
  xTaskNotifyStateClear(NULL); //forget a late echo from last time 

  gpio_set_level(PIN_TRIG, 1);
  esp_rom_delay_us(10); //busy-wait: far shorter than a tick 
  gpio_set_level(PIN_TRIG, 0);

  if (xTaskNotifyWait(0, UINT32_MAX, &width_us, pdMS_TO_TICKS(ECHO_TIMEOUT_MS)) 
  != pdTRUE) { 
    return false; //no echo came back
  }
  if (width_us < ECHO_MIN_US || width_us > ECHO_MAX_US) { 
    return false;
  }
  *cm = (float) width_us / US_PER_CM;
  return true;
}
