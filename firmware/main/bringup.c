#include "bringup.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "events.h"
#include "pump.h"

static const char *TAG = "bringup";

// Milestone 1: button press -> pour.
static void bringup_task(void *arg)
{
    event_t e;
    for (;;) {
        events_wait(&e, portMAX_DELAY);

        switch (e.type) {
        case EVT_BUTTON_POUR:
            ESP_LOGI(TAG, "button: requesting %d ms", POUR_FULL_MS);
            if (!pump_request(POUR_FULL_MS)) {
                ESP_LOGW(TAG, "pump busy, press ignored");
            }
            break;
        case EVT_BUTTON_HALF:
            ESP_LOGI(TAG, "half button: requesting %d ms", POUR_HALF_MS);
            pump_request(POUR_HALF_MS);
            break;
        case EVT_POUR_DONE:
        case EVT_POUR_ABORTED:
            ESP_LOGI(TAG, "pump reported %u ms", (unsigned)e.ms);
            break;
        default:
            break;
        }
    }
}

void bringup_start(void)
{
    xTaskCreatePinnedToCore(bringup_task, "bringup", 3072, NULL, 8, NULL, 1);
}