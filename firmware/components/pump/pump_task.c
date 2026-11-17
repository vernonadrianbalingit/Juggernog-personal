#include "pump.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "events.h"
#include "relay.h"

static const char *TAG = "pump";

typedef struct { 
  uint32_t duration_ms;
} pour_cmd_t;

static QueueHandle_t s_pour_q; //length 1: at most there is 1 pending pour
static TaskHandle_t s_pump_task;
static esp_timer_handle_t s_cutoff;
static volatile bool s_busy;

//2nd layer of preotection. runs in the esp_timer task and not the pump task 
// it will still fire if the pump task is stuck 
static void cutoff_cb(void *arg) { 
  relay_set(false);
  ESP_LOGE(TAG, "hard cutoff fired at %d ms", POUR_MAX_MS);
}

static void pump_task(void *arg) { 
  pour_cmd_t cmd;
  for (;;) { 
    //BLOCK HERE, uses no cpu, until someone calls pump_request()
    xQueueReceive(s_pour_q, &cmd, portMAX_DELAY);

    if(cmd.duration_ms == 0 || cmd.duration_ms > POUR_MAX_MS) { 
      ESP_LOGW(TAG, "rejected pour of %u ms", (unsigned)cmd.duration_ms);
      continue;
    }

    s_busy = true;
    ulTaskNotifyTake(pdTRUE,0); //drop a stale abort 
    esp_timer_start_once(s_cutoff, POUR_MAX_MS * 1000ULL); //arm layer 2 first 
    
    int64_t t0 = esp_timer_get_time();
    relay_set(true);

    //A delay that pump_abort() can cut short: returns if > 0 when notified 
    bool aborted = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(cmd.duration_ms)) > 0;
    relay_set(false);
    int64_t t1 = esp_timer_get_time();

    esp_timer_stop(s_cutoff);
    s_busy = false;

    event_t e = { 
      .type = aborted? EVT_POUR_ABORTED : EVT_POUR_DONE, 
      .t_us = t1, 
      .ms = (uint32_t)((t1-t0) / 1000),
    };
    events_post(&e);
    ESP_LOGI(TAG, "pour %s after %u ms", aborted ? "ABORTED" : "done", (unsigned)e.ms);
  }
}

bool pump_request(uint32_t ms) { 
  if (s_busy) { 
    return false;
  }
  pour_cmd_t c = { .duration_ms = ms };
  return xQueueSend(s_pour_q, &c, 0) == pdTRUE;
}

void pump_abort(void) { 
  if (s_busy) { 
    xTaskNotifyGive(s_pump_task);
  }
}

void pump_init(void) { 
  const esp_timer_create_args_t args = { 
    .callback = cutoff_cb,
    .name = "pour_cutoff"
  };
  ESP_ERROR_CHECK(esp_timer_create(&args, &s_cutoff));

  s_pour_q = xQueueCreate(1, sizeof(pour_cmd_t));
  configASSERT(s_pour_q);

  //Core 1, priority 10: THE HIGHEST OF ALL TASKs
  xTaskCreatePinnedToCore(pump_task, "pump", 3072, NULL, 10, &s_pump_task, 1);
}