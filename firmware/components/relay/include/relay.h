#pragma once 

#include <stdbool.h> //bool, for relay_set
#include "esp_err.h" // esp_err_t ,r eturned by relay_init

//Drive the realy pin to "off" and configure it as an output 
//call before anyting else in app_main 
esp_err_t relay_init(void);

//true = relay energised (pump rins), false = off. 
void relay_set(bool on);
// only the pump task and cutoff timer call this 