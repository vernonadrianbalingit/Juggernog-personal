#include "bringup.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "events.h"
#include "pump.h"

static const char *TAG = "bringup";

// Milestone 2: button press -> pour, but only with a cup in place.
// Pulling the cup away stops the pump.
static void bringup_task(void *arg) {
  bool cup = false;
  event_t e;
  for (;;) {
    events_wait(&e, portMAX_DELAY);

    switch (e.type) {
      case EVT_CUP_PRESENT:
        cup = true;
        break;
      case EVT_CUP_GONE:
        cup = false;
        pump_abort(); // does nothing if no pour is running
        break;
      case EVT_BUTTON_POUR:
      case EVT_BUTTON_HALF:
        if (!cup) {
          ESP_LOGW(TAG, "no cup, press ignored");
        } else if (!pump_request(e.type == EVT_BUTTON_HALF ? POUR_HALF_MS : POUR_FULL_MS)) {
          ESP_LOGW(TAG, "pump busy, press ignored");
        }
        break;
      case EVT_POUR_DONE:
      case EVT_POUR_ABORTED:
        ESP_LOGI(TAG, "pump reported %u ms%s", (unsigned)e.ms,
                 e.type == EVT_POUR_ABORTED ? " (aborted)" : "");
        break;
      default:
        break;
    }
  }
}

void bringup_start(void) {
  xTaskCreatePinnedToCore(bringup_task, "bringup", 3072, NULL, 8, NULL, 1);
}
