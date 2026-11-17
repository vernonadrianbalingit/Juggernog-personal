#pragma once 
#include "freertos/FreeRTOS.h"
#include "event_types.h" 

// The one queue every input will feed. EXACTLY ONE task reads it 

void events_init(void); //call once before any tasks start 
bool events_post(const event_t *e); //from a task; will never block 
bool events_post_from_isr(const event_t *e, BaseType_t *woken);
bool events_wait(event_t *out, TickType_t ticks); //false - timeout 