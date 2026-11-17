#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "board.h"
#include "bringup.h"
#include "buttons.h"
#include "events.h"
#include "pump.h"
#include "relay.h"

static const char *TAG = "main";

static void blink_task(void *arg){
  gpio_reset_pin(PIN_LED_STATUS);
  gpio_set_direction(PIN_LED_STATUS, GPIO_MODE_OUTPUT);
  for (;;) { 
    gpio_set_level(PIN_LED_STATUS, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_LED_STATUS, 0);
    vTaskDelay(pdMS_TO_TICKS(900));
  }
}

static const char *reset_reason_str(esp_reset_reason_t r) { 
  switch(r) { 
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_SW: return "SW";
    case ESP_RST_UNKNOWN: return "UNKNOWN";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_WDT: return "ESP_RST_WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    default: return "OTHER";
  }
}

void app_main(void)
{
  //pump off before everything else 
  ESP_ERROR_CHECK(relay_init());
  ESP_LOGI(TAG, "reset reason: %s", reset_reason_str(esp_reset_reason()));
  
  events_init();
  pump_init();
  buttons_init();
  bringup_start();
 
  xTaskCreatePinnedToCore(blink_task, "blink", 2048, NULL, 1, NULL, 1); 
}
