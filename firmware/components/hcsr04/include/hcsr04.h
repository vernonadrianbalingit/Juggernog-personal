#pragma once
#include <stdbool.h>

// Driver: one distance reading. Call only from the sensor task.
void hcsr04_init(void);              // needs the GPIO ISR service (buttons_init)
bool hcsr04_read_cm(float *cm);      // false = no echo or out of range

// Task: pings every 60 ms and posts EVT_CUP_PRESENT / EVT_CUP_GONE.
void sensor_task_start(void);