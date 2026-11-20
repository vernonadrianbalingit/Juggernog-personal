#include "cup_filter.h"

void cup_filter_init(cup_filter_t *f) {
  f->count = 0;
  f->next = 0;
  f->near_runs = 0;
  f->far_runs = 0;
  f->present = false;
}

float cup_filter_median(const cup_filter_t *f) { 
  if (f->count == 0) { 
    return -1.0f;
  }
  float s[CUP_WINDOW]; //s for finding the median
  for(int i = 0; i < f->count; i++) { 
    s[i] = f->win[i];
  }
  
  for(int i = 1; i < f->count; i++) { //insertion sort, 5 items max 
    float v = s[i];
    int j = i - 1; 

    while ( j >= 0 && s[j] > v) { 
      s[j+1] = s[j];
      j--;
    }
    s[j+1] = v;
  } 
  return s[f->count / 2];
}

cup_change_t cup_filter_update(cup_filter_t *f, bool valid, float cm) { 
  if (valid) {
    f->win[f->next] = cm;
    f->next = (f->next + 1) % CUP_WINDOW;
    if (f->count < CUP_WINDOW) {
      f->count++;
    }
    float m = cup_filter_median(f);
    if (m < CUP_NEAR_CM) { 
      f->near_runs++;
      f->far_runs = 0;
    } else if (m > CUP_FAR_CM) { 
      f->far_runs++;
      f->near_runs = 0;
    } else { 
      //Between the thresholds: hold the current state
      f->near_runs = 0;
      f->far_runs = 0;
    }
  } else { 
    //No echo. FAIL SAFE: count it as a "no cup" and forget old readings 
    //so stale near values ddo not keep the cup present
    f->count = 0;
    f->next = 0;
    f->far_runs++;
    f->near_runs =0;
  }

  if (!f->present && f->near_runs >= CUP_CONFIRM_RUNS) { 
    f->present = true;
    return CUP_BECAME_PRESENT;
  }
  if (f->present && f->far_runs >= CUP_CONFIRM_RUNS) { 
    f->present = false;
    return CUP_BECAME_GONE;
  }
  return CUP_NO_CHANGE;
}