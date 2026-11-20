#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cup_filter.h"
#include "events.h"
#include "hcsr04.h"

#define PING_PERIOD_MS 60       // lets the previous echo die out
#define LOG_DISTANCE   0        // 1 while tuning thresholds, 0 afterwards
#define LOG_EVERY      16       // print the median about once a second

static const char *TAG = "sensor";

static void sensor_task(void *arg){ 
  cup_filter_t filter;
  cup_filter_init(&filter);
  hcsr04_init();

  TickType_t last_wake = xTaskGetTickCount();
  unsigned n = 0;

  for (;;) { 
    float cm = 0;
    bool valid = hcsr04_read_cm(&cm);
    cup_change_t change = cup_filter_update(&filter, valid, cm);

    if(change != CUP_NO_CHANGE) { 
      event_t e = { 
        .type = (change == CUP_BECAME_PRESENT) ? EVT_CUP_PRESENT : EVT_CUP_GONE,
        .t_us = esp_timer_get_time(),
      };
      events_post(&e);
      ESP_LOGI(TAG, "cup %s", change == CUP_BECAME_PRESENT ? "PRESENT" : "GONE");
    }

    if (LOG_DISTANCE && ++n % LOG_EVERY == 0) {
      ESP_LOGI(TAG, "median %.1f cm, present=%d", cup_filter_median(&filter), filter.present);
    }

    //Fixed 60 ms period that is measued from last wakeup, so the time spent
    //measuring doesnt stretch the period
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(PING_PERIOD_MS));
  }
}

void sensor_task_start(void) { 
  xTaskCreatePinnedToCore(sensor_task, "sensor", 3072, NULL, 5, NULL, 1);
}

