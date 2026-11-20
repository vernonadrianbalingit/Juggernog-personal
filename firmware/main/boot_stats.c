#include "boot_stats.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "boot";

// RTC_NOINIT memory is not cleared by a reset, only by losing power
static RTC_NOINIT_ATTR uint32_t s_brownouts;

static const char *reset_reason_str(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_UNKNOWN:   return "UNKNOWN";
    default:                return "OTHER";
    }
}

void boot_stats_log(void)
{
    esp_reset_reason_t r = esp_reset_reason();
    if (r == ESP_RST_POWERON) {
        s_brownouts = 0;                // the memory will hold garbage after power-up
    } else if (r == ESP_RST_BROWNOUT) {
        s_brownouts++;
    }
    ESP_LOGW(TAG, "reset reason: %s, brownouts since power-on: %u",
             reset_reason_str(r), (unsigned)s_brownouts);
}

uint32_t boot_stats_brownouts(void)
{
    return s_brownouts;
}