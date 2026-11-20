#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "board.h"
#include "boot_stats.h"
#include "bringup.h"
#include "buttons.h"
#include "events.h"
#include "hcsr04.h"
#include "pn532.h"
#include "pump.h"
#include "relay.h"

static void blink_task(void *arg)
{
    gpio_reset_pin(PIN_LED_STATUS);
    gpio_set_direction(PIN_LED_STATUS, GPIO_MODE_OUTPUT);
    for (;;) {
        gpio_set_level(PIN_LED_STATUS, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(PIN_LED_STATUS, 0);
        vTaskDelay(pdMS_TO_TICKS(900));
    }
}

void app_main(void)
{
    // Pump off before anything else runs.
    ESP_ERROR_CHECK(relay_init());
    boot_stats_log();

    events_init();
    pump_init();
    buttons_init();          // also installs the GPIO ISR service
    sensor_task_start();
    nfc_task_start();        // retries on its own until the PN532 answers
    bringup_start();

    xTaskCreatePinnedToCore(blink_task, "blink", 2048, NULL, 1, NULL, 1);
}
