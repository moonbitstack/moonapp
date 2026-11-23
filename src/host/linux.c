/*
 * Which Linux desktop a process talks to, decided when the connection is made.
 *
 * A session is X11 or Wayland and a binary should not have to know in advance:
 * the two backends implement the same set of entry points under their own names,
 * and this file picks one, once, and forwards. `MOONAPP_BACKEND` names the choice
 * outright, which is what a test does when it wants a particular desktop rather
 * than whatever the session happens to be.
 */

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <moonbit.h>

#include "host_internal.h"

/* Both backends are compiled in, so both are named here. */
extern void * wl_host_new(void);
extern void * x11_host_new(void);
extern int32_t wl_host_ok(void *host);
extern int32_t x11_host_ok(void *host);
extern int32_t wl_host_error(void *host, char *into, int32_t cap);
extern int32_t x11_host_error(void *host, char *into, int32_t cap);
extern void wl_host_free(void *host);
extern void x11_host_free(void *host);
extern double wl_primary_scale(void *host);
extern double x11_primary_scale(void *host);
extern int32_t wl_is_shutdown(void *host);
extern int32_t x11_is_shutdown(void *host);
extern void wl_host_shutdown(void *host);
extern void x11_host_shutdown(void *host);
extern int32_t wl_open(void *host, const char *title, int32_t title_len, const char *app_id, int32_t app_id_len, int32_t width, int32_t height, int32_t flags);
extern int32_t x11_open(void *host, const char *title, int32_t title_len, const char *app_id, int32_t app_id_len, int32_t width, int32_t height, int32_t flags);
extern void wl_close(void *host, int32_t window);
extern void x11_close(void *host, int32_t window);
extern int32_t wl_window_open(void *host, int32_t window);
extern int32_t x11_window_open(void *host, int32_t window);
extern int32_t wl_poll(void *host);
extern int32_t x11_poll(void *host);
extern int32_t wl_wait(void *host, int32_t timeout_ms);
extern int32_t x11_wait(void *host, int32_t timeout_ms);
extern void wl_event_release(void *host);
extern void x11_event_release(void *host);
extern int32_t wl_event_kind(void *host);
extern int32_t x11_event_kind(void *host);
extern int32_t wl_event_window(void *host);
extern int32_t x11_event_window(void *host);
extern int32_t wl_event_i32(void *host, int32_t which);
extern int32_t x11_event_i32(void *host, int32_t which);
extern double wl_event_f64(void *host, int32_t which);
extern double x11_event_f64(void *host, int32_t which);
extern int32_t wl_event_text_len(void *host);
extern int32_t x11_event_text_len(void *host);
extern int32_t wl_event_text(void *host, uint8_t *buffer, int32_t cap);
extern int32_t x11_event_text(void *host, uint8_t *buffer, int32_t cap);
extern void wl_set_title(void *host, int32_t window, const char *title, int32_t len);
extern void x11_set_title(void *host, int32_t window, const char *title, int32_t len);
extern void wl_set_size(void *host, int32_t window, int32_t width, int32_t height);
extern void x11_set_size(void *host, int32_t window, int32_t width, int32_t height);
extern void wl_set_min_size(void *host, int32_t window, int32_t width, int32_t height);
extern void x11_set_min_size(void *host, int32_t window, int32_t width, int32_t height);
extern int32_t wl_width(void *host, int32_t window);
extern int32_t x11_width(void *host, int32_t window);
extern int32_t wl_height(void *host, int32_t window);
extern int32_t x11_height(void *host, int32_t window);
extern double wl_scale(void *host, int32_t window);
extern double x11_scale(void *host, int32_t window);
extern void wl_set_cursor(void *host, int32_t window, int32_t cursor);
extern void x11_set_cursor(void *host, int32_t window, int32_t cursor);
extern int32_t wl_present(void *host, int32_t window, uint8_t *bgra, int32_t width, int32_t height, int32_t stride);
extern int32_t x11_present(void *host, int32_t window, uint8_t *bgra, int32_t width, int32_t height, int32_t stride);
extern int32_t wl_read_back(void *host, int32_t window, uint8_t *out, int32_t cap);
extern int32_t x11_read_back(void *host, int32_t window, uint8_t *out, int32_t cap);
extern int32_t wl_clipboard_get(void *host, uint8_t *buffer, int32_t cap);
extern int32_t x11_clipboard_get(void *host, uint8_t *buffer, int32_t cap);
extern void wl_clipboard_set(void *host, const char *text, int32_t len);
extern void x11_clipboard_set(void *host, const char *text, int32_t len);
extern void wl_set_ime_allowed(void *host, int32_t window, int32_t allowed);
extern void x11_set_ime_allowed(void *host, int32_t window, int32_t allowed);
extern void wl_set_ime_area(void *host, int32_t window, int32_t x, int32_t y, int32_t width, int32_t height);
extern void x11_set_ime_area(void *host, int32_t window, int32_t x, int32_t y, int32_t width, int32_t height);
extern int32_t wl_native_kind(void);
extern int32_t x11_native_kind(void);
extern uint64_t wl_native_display(void *host);
extern uint64_t x11_native_display(void *host);
extern uint64_t wl_native_window(void *host, int32_t window);
extern uint64_t x11_native_window(void *host, int32_t window);
extern int32_t wl_test_key(void *host, int32_t window, int32_t code, int32_t down);
extern int32_t x11_test_key(void *host, int32_t window, int32_t code, int32_t down);
extern int32_t wl_test_text(void *host, int32_t window, int32_t unit);
extern int32_t x11_test_text(void *host, int32_t window, int32_t unit);
extern int32_t wl_test_pointer(void *host, int32_t window, int32_t kind, int32_t x, int32_t y, int32_t button);
extern int32_t x11_test_pointer(void *host, int32_t window, int32_t kind, int32_t x, int32_t y, int32_t button);

typedef struct moonapp_host {
  int32_t wayland;
  void *inner;
} moonapp_host;

#define INNER(host) (((moonapp_host *)(host))->inner)
#define IS_WAYLAND(host) (((moonapp_host *)(host))->wayland)

static int32_t kind_of_session = MOONAPP_PLATFORM_X11;

MOONBIT_FFI_EXPORT
void *moonapp_host_new(void) {
  const char *forced = getenv("MOONAPP_BACKEND");
  int32_t wayland = -1;
  if (forced != NULL && strcmp(forced, "wayland") == 0) wayland = 1;
  if (forced != NULL && strcmp(forced, "x11") == 0) wayland = 0;
  if (wayland < 0) {
    const char *session = getenv("WAYLAND_DISPLAY");
    wayland = session != NULL && session[0] != 0;
  }
  void *inner = NULL;
  if (wayland) {
    inner = wl_host_new();
    if (inner == NULL) {
      inner = x11_host_new();
      wayland = 0;
    }
  } else {
    inner = x11_host_new();
    if (inner == NULL) {
      inner = wl_host_new();
      wayland = 1;
    }
  }
  if (inner == NULL) return NULL;
  moonapp_host *host = (moonapp_host *)calloc(1, sizeof(moonapp_host));
  if (host == NULL) {
    if (wayland) wl_host_free(inner);
    else x11_host_free(inner);
    return NULL;
  }
  host->wayland = wayland;
  host->inner = inner;
  kind_of_session = wayland ? MOONAPP_PLATFORM_WAYLAND : MOONAPP_PLATFORM_X11;
  return host;
}
MOONBIT_FFI_EXPORT
int32_t moonapp_host_ok(void *host) {
  return host != NULL && INNER(host) != NULL;
}
MOONBIT_FFI_EXPORT
int32_t moonapp_host_error(void *host, char *into, int32_t cap) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_host_error(INNER(host), into, cap) : x11_host_error(INNER(host), into, cap);
}
MOONBIT_FFI_EXPORT
void moonapp_host_free(void *host) {
  if (host == NULL) return;
  if (IS_WAYLAND(host)) wl_host_free(INNER(host));
  else x11_host_free(INNER(host));
  free(host);
}
MOONBIT_FFI_EXPORT
double moonapp_primary_scale(void *host) {
  if (host == NULL) return 1.0;
  return IS_WAYLAND(host) ? wl_primary_scale(INNER(host)) : x11_primary_scale(INNER(host));
}
MOONBIT_FFI_EXPORT
int32_t moonapp_is_shutdown(void *host) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_is_shutdown(INNER(host)) : x11_is_shutdown(INNER(host));
}
MOONBIT_FFI_EXPORT
void moonapp_host_shutdown(void *host) {
  if (host == NULL) return;
  if (IS_WAYLAND(host)) wl_host_shutdown(INNER(host));
  else x11_host_shutdown(INNER(host));
}
MOONBIT_FFI_EXPORT
int32_t moonapp_open(void *host, const char *title, int32_t title_len, const char *app_id, int32_t app_id_len, int32_t width, int32_t height, int32_t flags) {
  if (host == NULL) return -1;
  return IS_WAYLAND(host) ? wl_open(INNER(host), title, title_len, app_id, app_id_len, width, height, flags) : x11_open(INNER(host), title, title_len, app_id, app_id_len, width, height, flags);
}
MOONBIT_FFI_EXPORT
void moonapp_close(void *host, int32_t window) {
  if (host == NULL) return;
  if (IS_WAYLAND(host)) wl_close(INNER(host), window);
  else x11_close(INNER(host), window);
}
MOONBIT_FFI_EXPORT
int32_t moonapp_window_open(void *host, int32_t window) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_window_open(INNER(host), window) : x11_window_open(INNER(host), window);
}
MOONBIT_FFI_EXPORT
int32_t moonapp_poll(void *host) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_poll(INNER(host)) : x11_poll(INNER(host));
}
MOONBIT_FFI_EXPORT
int32_t moonapp_wait(void *host, int32_t timeout_ms) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_wait(INNER(host), timeout_ms) : x11_wait(INNER(host), timeout_ms);
}
MOONBIT_FFI_EXPORT
void moonapp_event_release(void *host) {
  if (host == NULL) return;
  if (IS_WAYLAND(host)) wl_event_release(INNER(host));
  else x11_event_release(INNER(host));
}
MOONBIT_FFI_EXPORT
int32_t moonapp_event_kind(void *host) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_event_kind(INNER(host)) : x11_event_kind(INNER(host));
}
MOONBIT_FFI_EXPORT
int32_t moonapp_event_window(void *host) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_event_window(INNER(host)) : x11_event_window(INNER(host));
}
MOONBIT_FFI_EXPORT
int32_t moonapp_event_i32(void *host, int32_t which) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_event_i32(INNER(host), which) : x11_event_i32(INNER(host), which);
}
MOONBIT_FFI_EXPORT
double moonapp_event_f64(void *host, int32_t which) {
  if (host == NULL) return 1.0;
  return IS_WAYLAND(host) ? wl_event_f64(INNER(host), which) : x11_event_f64(INNER(host), which);
}
MOONBIT_FFI_EXPORT
int32_t moonapp_event_text_len(void *host) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_event_text_len(INNER(host)) : x11_event_text_len(INNER(host));
}
MOONBIT_FFI_EXPORT
int32_t moonapp_event_text(void *host, uint8_t *buffer, int32_t cap) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_event_text(INNER(host), buffer, cap) : x11_event_text(INNER(host), buffer, cap);
}
MOONBIT_FFI_EXPORT
void moonapp_set_title(void *host, int32_t window, const char *title, int32_t len) {
  if (host == NULL) return;
  if (IS_WAYLAND(host)) wl_set_title(INNER(host), window, title, len);
  else x11_set_title(INNER(host), window, title, len);
}
MOONBIT_FFI_EXPORT
void moonapp_set_size(void *host, int32_t window, int32_t width, int32_t height) {
  if (host == NULL) return;
  if (IS_WAYLAND(host)) wl_set_size(INNER(host), window, width, height);
  else x11_set_size(INNER(host), window, width, height);
}
MOONBIT_FFI_EXPORT
void moonapp_set_min_size(void *host, int32_t window, int32_t width, int32_t height) {
  if (host == NULL) return;
  if (IS_WAYLAND(host)) wl_set_min_size(INNER(host), window, width, height);
  else x11_set_min_size(INNER(host), window, width, height);
}
MOONBIT_FFI_EXPORT
int32_t moonapp_width(void *host, int32_t window) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_width(INNER(host), window) : x11_width(INNER(host), window);
}
MOONBIT_FFI_EXPORT
int32_t moonapp_height(void *host, int32_t window) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_height(INNER(host), window) : x11_height(INNER(host), window);
}
MOONBIT_FFI_EXPORT
double moonapp_scale(void *host, int32_t window) {
  if (host == NULL) return 1.0;
  return IS_WAYLAND(host) ? wl_scale(INNER(host), window) : x11_scale(INNER(host), window);
}
MOONBIT_FFI_EXPORT
void moonapp_set_cursor(void *host, int32_t window, int32_t cursor) {
  if (host == NULL) return;
  if (IS_WAYLAND(host)) wl_set_cursor(INNER(host), window, cursor);
  else x11_set_cursor(INNER(host), window, cursor);
}
MOONBIT_FFI_EXPORT
int32_t moonapp_present(void *host, int32_t window, uint8_t *bgra, int32_t width, int32_t height, int32_t stride) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_present(INNER(host), window, bgra, width, height, stride) : x11_present(INNER(host), window, bgra, width, height, stride);
}
MOONBIT_FFI_EXPORT
int32_t moonapp_read_back(void *host, int32_t window, uint8_t *out, int32_t cap) {
  if (host == NULL) return -1;
  return IS_WAYLAND(host) ? wl_read_back(INNER(host), window, out, cap) : x11_read_back(INNER(host), window, out, cap);
}
MOONBIT_FFI_EXPORT
int32_t moonapp_clipboard_get(void *host, uint8_t *buffer, int32_t cap) {
  if (host == NULL) return -1;
  return IS_WAYLAND(host) ? wl_clipboard_get(INNER(host), buffer, cap) : x11_clipboard_get(INNER(host), buffer, cap);
}
MOONBIT_FFI_EXPORT
void moonapp_clipboard_set(void *host, const char *text, int32_t len) {
  if (host == NULL) return;
  if (IS_WAYLAND(host)) wl_clipboard_set(INNER(host), text, len);
  else x11_clipboard_set(INNER(host), text, len);
}
MOONBIT_FFI_EXPORT
void moonapp_set_ime_allowed(void *host, int32_t window, int32_t allowed) {
  if (host == NULL) return;
  if (IS_WAYLAND(host)) wl_set_ime_allowed(INNER(host), window, allowed);
  else x11_set_ime_allowed(INNER(host), window, allowed);
}
MOONBIT_FFI_EXPORT
void moonapp_set_ime_area(void *host, int32_t window, int32_t x, int32_t y, int32_t width, int32_t height) {
  if (host == NULL) return;
  if (IS_WAYLAND(host)) wl_set_ime_area(INNER(host), window, x, y, width, height);
  else x11_set_ime_area(INNER(host), window, x, y, width, height);
}
MOONBIT_FFI_EXPORT
uint64_t moonapp_native_display(void *host) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_native_display(INNER(host)) : x11_native_display(INNER(host));
}
MOONBIT_FFI_EXPORT
uint64_t moonapp_native_window(void *host, int32_t window) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_native_window(INNER(host), window) : x11_native_window(INNER(host), window);
}
MOONBIT_FFI_EXPORT
int32_t moonapp_test_key(void *host, int32_t window, int32_t code, int32_t down) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_test_key(INNER(host), window, code, down) : x11_test_key(INNER(host), window, code, down);
}
MOONBIT_FFI_EXPORT
int32_t moonapp_test_text(void *host, int32_t window, int32_t unit) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_test_text(INNER(host), window, unit) : x11_test_text(INNER(host), window, unit);
}
MOONBIT_FFI_EXPORT
int32_t moonapp_test_pointer(void *host, int32_t window, int32_t kind, int32_t x, int32_t y, int32_t button) {
  if (host == NULL) return 0;
  return IS_WAYLAND(host) ? wl_test_pointer(INNER(host), window, kind, x, y, button) : x11_test_pointer(INNER(host), window, kind, x, y, button);
}
MOONBIT_FFI_EXPORT
int32_t moonapp_native_kind(void) { return kind_of_session; }

#endif /* __linux__ || __unix__ */
