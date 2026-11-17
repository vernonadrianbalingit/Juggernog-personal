#pragma once 
#include <stdbool.h>
#include <stdint.h> 

#define POUR_MAX_MS 5000 //hard ceiling
#define POUR_FULL_MS 500 //replace with calibrated time for fulll shot 
#define POUR_HALF_MS 750 //half shot 

void pump_init(void); //call after relay_init() and events_init()
bool pump_request(uint32_t ms); //false if a pour is already queued or running 
void pump_abort(void); //stop the current pour ("eearly cup lift off")

