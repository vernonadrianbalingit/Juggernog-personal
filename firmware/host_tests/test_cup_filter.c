#include "check.h"
#include "../components/hcsr04/cup_filter.c" //pull in the code being tested
#include "cup_filter.h"

//Feed the same reading in a bunch of times, return the last change it caused
static cup_change_t feed(cup_filter_t *f, float cm, int times) {
  cup_change_t last = CUP_NO_CHANGE;
  for (int i = 0; i < times; i++) {
    cup_change_t c = cup_filter_update(f, true, cm);
    if (c != CUP_NO_CHANGE) {
      last = c;
    }
  }
  return last;
}

int main(void) {
  cup_filter_t f;

  //needs 3 near readings in a row to say present, 2 isnt enough
  cup_filter_init(&f);
  CHECK(cup_filter_update(&f, true, 5.0f) == CUP_NO_CHANGE);
  CHECK(cup_filter_update(&f, true, 5.0f) == CUP_NO_CHANGE);
  CHECK(cup_filter_update(&f, true, 5.0f) == CUP_BECAME_PRESENT);
  CHECK(f.present);

  //only reports the change once, not every reading after
  CHECK(cup_filter_update(&f, true, 5.0f) == CUP_NO_CHANGE);

  //one wild reading cant move the median so the cup stays
  CHECK(cup_filter_update(&f, true, 300.0f) == CUP_NO_CHANGE);
  CHECK(f.present);
  CHECK(cup_filter_median(&f) == 5.0f);

  //in between the thresholds (8 to 12 cm) nothing changes, thats the hysteresis
  cup_filter_init(&f);
  feed(&f, 5.0f, 5);
  CHECK(feed(&f, 10.0f, 10) == CUP_NO_CHANGE);
  CHECK(f.present);

  //cup pulled away: far readings take over the median, then 3 in a row = gone
  CHECK(feed(&f, 40.0f, 10) == CUP_BECAME_GONE);
  CHECK(!f.present);

  //no echo counts as no cup after 3 misses, fail safe so the pump cant run blind
  cup_filter_init(&f);
  feed(&f, 5.0f, 5);
  CHECK(cup_filter_update(&f, false, 0) == CUP_NO_CHANGE);
  CHECK(cup_filter_update(&f, false, 0) == CUP_NO_CHANGE);
  CHECK(cup_filter_update(&f, false, 0) == CUP_BECAME_GONE);

  //empty filter has no median, should give -1
  cup_filter_init(&f);
  CHECK(cup_filter_median(&f) == -1.0f);

  CHECK_SUMMARY();
}
