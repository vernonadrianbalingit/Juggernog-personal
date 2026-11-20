#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "events.h"
#include "pn532.h"

#define POLL_PERIOD_MS   100
#define REINIT_PERIOD_MS 2000

static const char *TAG = "nfc";

static volatile bool s_ready;

bool nfc_is_ready(void) {
  return s_ready;
}

static void nfc_task(void *arg) {
  //keep trying until the reader answers, nothing else is waiting on this task
  while (pn532_init() != ESP_OK) {
    ESP_LOGE(TAG, "PN532 not ready, retrying");
    vTaskDelay(pdMS_TO_TICKS(REINIT_PERIOD_MS));
  }
  s_ready = true;

  uint8_t last[UID_MAX_LEN];
  uint8_t last_len = 0;   //0 = no tag was in the field on the last poll

  for (;;) {
    uint8_t uid[UID_MAX_LEN];
    uint8_t len = 0;
    bool found = false;

    if (pn532_read_uid(uid, &len, &found) != ESP_OK) {
      ESP_LOGW(TAG, "read failed");
    } else if (!found) {
      last_len = 0;   //tag left, so the same cup is allowed to scan again
    } else if (len != last_len || memcmp(uid, last, len) != 0) {
      //a tag that was not here last poll, report it once and remember it
      event_t e = { .type = EVT_TAG, .t_us = esp_timer_get_time(), .uid_len = len };
      memcpy(e.uid, uid, len);
      events_post(&e);

      memcpy(last, uid, len);
      last_len = len;
    }

    vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));
  }
}

void nfc_task_start(void) {
  xTaskCreatePinnedToCore(nfc_task, "nfc", 4096, NULL, 4, NULL, 1);
}
