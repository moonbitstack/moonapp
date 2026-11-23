/*
 * The part of a backend that does not depend on the desktop: the event record
 * that crosses into MoonBit, the queue that holds it, and the text a backend
 * copies into it.
 *
 * Each desktop has one C file that implements the same set of `moonapp_*`
 * entry points, chosen at compile time by the preprocessor guard at the top of
 * that file. The MoonBit side declares those entry points once, so nothing above
 * this layer knows which desktop it is running on.
 */

#ifndef MOONAPP_HOST_INTERNAL_H
#define MOONAPP_HOST_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Events waiting for the application, and windows one host may own. A queue
 * that fills up drops the oldest event rather than blocking the desktop, which
 * is what a platform message loop would do anyway when it stops reading. */
#define MOONAPP_QUEUE 64
#define MOONAPP_MAX_WINDOWS 8
#define MOONAPP_TEXT 4096
#define MOONAPP_ERROR 512

enum {
  MOONAPP_EV_RESIZED = 0,
  MOONAPP_EV_SCALE = 1,
  MOONAPP_EV_FOCUSED = 2,
  MOONAPP_EV_CLOSE = 3,
  MOONAPP_EV_KEY_DOWN = 4,
  MOONAPP_EV_KEY_UP = 5,
  MOONAPP_EV_TEXT = 6,
  MOONAPP_EV_IME_PREEDIT = 7,
  MOONAPP_EV_IME_COMMIT = 8,
  MOONAPP_EV_IME_ENABLED = 9,
  MOONAPP_EV_IME_DISABLED = 10,
  MOONAPP_EV_POINTER_MOVED = 11,
  MOONAPP_EV_POINTER_ENTERED = 12,
  MOONAPP_EV_POINTER_LEFT = 13,
  MOONAPP_EV_BUTTON_DOWN = 14,
  MOONAPP_EV_BUTTON_UP = 15,
  MOONAPP_EV_WHEEL = 16,
  MOONAPP_EV_GESTURE = 17
};

/* Window flags, as `moonapp_open` receives them. */
#define MOONAPP_FLAG_DECORATIONS 1
#define MOONAPP_FLAG_RESIZABLE 2
#define MOONAPP_FLAG_TRANSPARENT 4
#define MOONAPP_FLAG_VISIBLE 8
#define MOONAPP_FLAG_HIGH_DPI 16

/* Which desktop, as `moonapp_native_kind` reports it. */
#define MOONAPP_PLATFORM_WINDOWS 0
#define MOONAPP_PLATFORM_MACOS 1
#define MOONAPP_PLATFORM_X11 2
#define MOONAPP_PLATFORM_WAYLAND 3

typedef struct moonapp_event {
  int32_t kind;
  int32_t window;
  int32_t i32a;
  int32_t i32b;
  int32_t i32c;
  int32_t i32d;
  double f64a;
  double f64b;
  int32_t text_len;
  char text[MOONAPP_TEXT];
} moonapp_event;

typedef struct moonapp_queue {
  moonapp_event items[MOONAPP_QUEUE];
  int32_t head;
  int32_t count;
} moonapp_queue;

static inline void moonapp_queue_init(moonapp_queue *queue) {
  queue->head = 0;
  queue->count = 0;
}

/* The slot to write a new event into, or NULL when the queue is full. */
static inline moonapp_event *moonapp_queue_push(moonapp_queue *queue) {
  if (queue->count == MOONAPP_QUEUE) return NULL;
  int32_t at = (queue->head + queue->count) % MOONAPP_QUEUE;
  queue->count++;
  moonapp_event *event = &queue->items[at];
  memset(event, 0, sizeof(*event));
  return event;
}

static inline int32_t moonapp_queue_pop(moonapp_queue *queue,
                                       moonapp_event *out) {
  if (queue->count == 0) return 0;
  *out = queue->items[queue->head];
  queue->head = (queue->head + 1) % MOONAPP_QUEUE;
  queue->count--;
  return 1;
}

/* Text goes into the event as UTF-8, cut at a character boundary if it does not
 * fit: half a character would be worse than a short one. */
static inline void moonapp_event_set_text(moonapp_event *event,
                                          const char *text, int32_t len) {
  int32_t cap = MOONAPP_TEXT - 1;
  if (text == NULL || len <= 0) {
    event->text_len = 0;
    event->text[0] = 0;
    return;
  }
  if (len > cap) {
    len = cap;
    while (len > 0 && ((unsigned char)text[len] & 0xC0) == 0x80) len--;
  }
  memcpy(event->text, text, (size_t)len);
  event->text[len] = 0;
  event->text_len = len;
}

static inline void moonapp_error(char *into, const char *message) {
  if (into == NULL) return;
  if (message == NULL) message = "";
  size_t len = strlen(message);
  if (len >= MOONAPP_ERROR) len = MOONAPP_ERROR - 1;
  memcpy(into, message, len);
  into[len] = 0;
}

#endif
