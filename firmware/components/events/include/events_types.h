#pragma once 
#include <stdbool.h>
#include <stdint.h> 

//Plain data types shared by every task, NO FreeRTOS yet
//session logic that uses these types will compoile on the mac too for tests 

#define UID_MAX_LEN 7 

typedef enum {
  EVT_BUTTON_POUR, //pour button pressed
  EVT_BUTTON_HALF, //half-shot button pressed
  EVT_CUP_PRESENT, //sensor task: a cup arrived 
  EVT_CUP_GONE, //sensor task: the cup left 
  EVT_TAG, //NFC task: a new tag was scanned (uid, uid_lenset)
  EVT_POUR_DONE, //pump task: pour ran its full time (ms set) 
  EVT_POUR_ABORTED, //pump task: pour was cut short (ms set) 
  EVT_TIMEOUTT, //made up by the controller when a wait expires
} evt_type_t;

typedef struct { 
  evt_type_t type;
  int64_t t_us; //esp_timer_get_time() when it happend
  uint32_t ms; //EVT_POUR_* how long the pump really ran 
  uint8_t uid[UID_MAX_LEN]; //EVT_TAG only 
  uint8_t uid_len; //EVT_TAG only: 4 or 7 
} event_t;

//WHat the UI task can show or play 
typedef enum { 
  CUE_SCANNED,
  CUE_CANCELLED,
  CUE_SIZE_CHANGED,
  CUE_POURING,
  CUE_DONE,
  CUE_ERROR,
}cue_t;

//One finished pour, handed to the uplink task for logging 
typedef struct { 
  uint8_t uid[UID_MAX_LEN];
  uint8_t uid_len;
  uint16_t pour_ms;
  bool half; 
  bool aborted;
  int64_t t_us; //when did the pour end 
} pour_record_t;