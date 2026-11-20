#pragma once
#include <stdint.h>

// Logs why the chip reset and keeps a brownout count that survives resets.
void     boot_stats_log(void);
uint32_t boot_stats_brownouts(void);