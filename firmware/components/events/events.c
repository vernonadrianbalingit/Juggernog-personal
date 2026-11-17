#include "events.h"
#include "freertos/queue.h"

#define EVENT_QUEUE_LEN 16 

static QueueHandle_t s_q;

void events_init(void) { 
  s_q = xQueueCreate(EVENT_QUEUE_LEN, sizeof(event_t));
  configASSERT(s_q);
}

bool events_post(const event_t *e) {
  return xQueueSend(s_q, e, 0) == pdTRUE;
}

bool events_post_from_isr(const event_t *e, BaseType_t *woken) { 
  return xQueueSendFromISR(s_q, e, woken) == pdTRUE;
}

bool events_wait(event_t *out, TickType_t ticks) {
  return xQueueReceive(s_q, out, ticks) == pdTRUE;
}