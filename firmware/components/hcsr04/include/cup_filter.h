#pragma once 
#include <stdbool.h> 

//Turn noisyh distance readingt to a clean or present or gone decision 
//Pure c with no hardware so i can test it on my mac 

#define CUP_NEAR_CM 8.0f //close than this = cup is there 
#define CUP_FAR_CM 12.0f //farter than this = no cup 
#define CUP_CONFIRM_RUNS 3 //readins in a row needed to alter state
#define CUP_WINDOW 5 //median winodw 

typedef enum {
  CUP_NO_CHANGE,
  CUP_BECAME_PRESENT,
  CUP_BECAME_GONE,
} cup_change_t;


typedef struct { 
  float win[CUP_WINDOW];
  int count; //valid readings stored so far, goes up to CUP_WINDOW
  int next; //slot for next reading to go into 
  int near_runs;
  int far_runs;
  bool present;
} cup_filter_t;

void cup_filter_init(cup_filter_t *f);
cup_change_t cup_filter_update(cup_filter_t *f, bool valid, float cm);
float cup_filter_median(const cup_filter_t *f); //-1 if empty 

