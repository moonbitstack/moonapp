/*
 * Wayland backend.
 *
 * Nothing here is linked against, or included from, the system's Wayland
 * headers. The client library, the core protocol's own interface tables, and the
 * keyboard layout library are all resolved at run time; the xdg-shell bindings
 * below are what a protocol scanner would generate from the official XML. A
 * build therefore needs no development package, and a machine with a compositor
 * running has everything this file needs.
 */

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <moonbit.h>

#include "host_internal.h"

/* ------------------------------------------------------------------------- */
/* The pieces of the Wayland ABI this file speaks                             */
/* ------------------------------------------------------------------------- */

struct wl_display;
struct wl_proxy;
struct wl_interface;
struct wl_message;
struct wl_array {
  size_t size;
  size_t alloc;
  void *data;
};

struct wl_list {
  struct wl_list *prev;
  struct wl_list *next;
};

struct wl_listener {
  struct wl_list link;
  void (*notify)(struct wl_listener *, void *);
};

struct wl_interface {
  const char *name;
  int version;
  int method_count;
  const struct wl_message *methods;
  int event_count;
  const struct wl_message *events;
};

struct wl_message {
  const char *name;
  const char *signature;
  const struct wl_interface **types;
};

#define WL_MARSHAL_FLAG_DESTROY 1

typedef int32_t wl_fixed_t;

static struct wl_interface *wl_registry_interface;
static struct wl_interface *wl_compositor_interface;
static struct wl_interface *wl_surface_interface;
static struct wl_interface *wl_shm_interface;
static struct wl_interface *wl_shm_pool_interface;
static struct wl_interface *wl_buffer_interface;
static struct wl_interface *wl_seat_interface;
static struct wl_interface *wl_pointer_interface;
static struct wl_interface *wl_keyboard_interface;
static struct wl_interface *wl_output_interface;
static struct wl_interface *wl_data_device_manager_interface;
static struct wl_interface *wl_data_device_interface;
static struct wl_interface *wl_data_source_interface;
static struct wl_interface *wl_data_offer_interface;

static struct wl_display *(*p_wl_display_connect)(const char *);
static void (*p_wl_display_disconnect)(struct wl_display *);
static int (*p_wl_display_dispatch)(struct wl_display *);
static int (*p_wl_display_dispatch_pending)(struct wl_display *);
static int (*p_wl_display_flush)(struct wl_display *);
static int (*p_wl_display_roundtrip)(struct wl_display *);
static int (*p_wl_display_prepare_read)(struct wl_display *);
static int (*p_wl_display_read_events)(struct wl_display *);
static void (*p_wl_display_cancel_read)(struct wl_display *);
static int (*p_wl_display_get_fd)(struct wl_display *);
static struct wl_proxy *(*p_wl_proxy_marshal_flags)(
    struct wl_proxy *, uint32_t, const struct wl_interface *, uint32_t, uint32_t,
    ...);
static int (*p_wl_proxy_add_listener)(struct wl_proxy *,
                                      void (**)(void), void *);
static uint32_t (*p_wl_proxy_get_version)(struct wl_proxy *);
static void (*p_wl_proxy_destroy)(struct wl_proxy *);

/* xkbcommon, for turning an evdev key into a key code and a character. */
static struct xkb_context *(*p_xkb_context_new)(int);
static void (*p_xkb_context_unref)(struct xkb_context *);
static struct xkb_keymap *(*p_xkb_keymap_new_from_string)(struct xkb_context *,
                                                         const char *, int,
                                                         int);
static void (*p_xkb_keymap_unref)(struct xkb_keymap *);
static struct xkb_state *(*p_xkb_state_new)(struct xkb_keymap *);
static void (*p_xkb_state_unref)(struct xkb_state *);
static uint32_t (*p_xkb_state_key_get_one_sym)(struct xkb_state *, uint32_t);
static int (*p_xkb_state_key_get_utf8)(struct xkb_state *, uint32_t, char *,
                                       size_t);
static uint32_t (*p_xkb_state_update_mask)(struct xkb_state *, uint32_t,
                                           uint32_t, uint32_t, uint32_t,
                                           uint32_t, uint32_t);

static void *wayland_library;
static void *xkb_library;

static void *lookup(void *library, const char *name) {
  return dlsym(library, name);
}

static int32_t load_wayland(void) {
  if (wayland_library != NULL) return 1;
  static const char *names[] = { "libwayland-client.so.0",
                                 "libwayland-client.so", NULL };
  for (int32_t i = 0; names[i] != NULL && wayland_library == NULL; i++) {
    wayland_library = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
  }
  if (wayland_library == NULL) return 0;
  *(void **)(&p_wl_display_connect) = lookup(wayland_library, "wl_display_connect");
  *(void **)(&p_wl_display_disconnect) =
      lookup(wayland_library, "wl_display_disconnect");
  *(void **)(&p_wl_display_dispatch) =
      lookup(wayland_library, "wl_display_dispatch");
  *(void **)(&p_wl_display_dispatch_pending) =
      lookup(wayland_library, "wl_display_dispatch_pending");
  *(void **)(&p_wl_display_flush) = lookup(wayland_library, "wl_display_flush");
  *(void **)(&p_wl_display_roundtrip) =
      lookup(wayland_library, "wl_display_roundtrip");
  *(void **)(&p_wl_display_prepare_read) =
      lookup(wayland_library, "wl_display_prepare_read");
  *(void **)(&p_wl_display_read_events) =
      lookup(wayland_library, "wl_display_read_events");
  *(void **)(&p_wl_display_cancel_read) =
      lookup(wayland_library, "wl_display_cancel_read");
  *(void **)(&p_wl_display_get_fd) = lookup(wayland_library, "wl_display_get_fd");
  *(void **)(&p_wl_proxy_marshal_flags) =
      lookup(wayland_library, "wl_proxy_marshal_flags");
  *(void **)(&p_wl_proxy_add_listener) =
      lookup(wayland_library, "wl_proxy_add_listener");
  *(void **)(&p_wl_proxy_get_version) =
      lookup(wayland_library, "wl_proxy_get_version");
  *(void **)(&p_wl_proxy_destroy) = lookup(wayland_library, "wl_proxy_destroy");
  if (p_wl_display_connect == NULL || p_wl_proxy_marshal_flags == NULL ||
      p_wl_proxy_add_listener == NULL || p_wl_proxy_get_version == NULL ||
      p_wl_proxy_destroy == NULL || p_wl_display_flush == NULL ||
      p_wl_display_roundtrip == NULL || p_wl_display_prepare_read == NULL ||
      p_wl_display_read_events == NULL || p_wl_display_cancel_read == NULL ||
      p_wl_display_dispatch_pending == NULL || p_wl_display_get_fd == NULL) {
    return 0;
  }
  *(void **)(&wl_registry_interface) = lookup(wayland_library, "wl_registry_interface");
  *(void **)(&wl_compositor_interface) =
      lookup(wayland_library, "wl_compositor_interface");
  *(void **)(&wl_surface_interface) = lookup(wayland_library, "wl_surface_interface");
  *(void **)(&wl_shm_interface) = lookup(wayland_library, "wl_shm_interface");
  *(void **)(&wl_shm_pool_interface) =
      lookup(wayland_library, "wl_shm_pool_interface");
  *(void **)(&wl_buffer_interface) = lookup(wayland_library, "wl_buffer_interface");
  *(void **)(&wl_seat_interface) = lookup(wayland_library, "wl_seat_interface");
  *(void **)(&wl_pointer_interface) = lookup(wayland_library, "wl_pointer_interface");
  *(void **)(&wl_keyboard_interface) =
      lookup(wayland_library, "wl_keyboard_interface");
  *(void **)(&wl_output_interface) = lookup(wayland_library, "wl_output_interface");
  *(void **)(&wl_data_device_manager_interface) =
      lookup(wayland_library, "wl_data_device_manager_interface");
  *(void **)(&wl_data_device_interface) =
      lookup(wayland_library, "wl_data_device_interface");
  *(void **)(&wl_data_source_interface) =
      lookup(wayland_library, "wl_data_source_interface");
  *(void **)(&wl_data_offer_interface) =
      lookup(wayland_library, "wl_data_offer_interface");
  return wl_compositor_interface != NULL && wl_surface_interface != NULL &&
         wl_shm_interface != NULL;
}

static int32_t load_xkb(void) {
  if (xkb_library != NULL) return 1;
  static const char *names[] = { "libxkbcommon.so.0", "libxkbcommon.so", NULL };
  for (int32_t i = 0; names[i] != NULL && xkb_library == NULL; i++) {
    xkb_library = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
  }
  if (xkb_library == NULL) return 0;
  *(void **)(&p_xkb_context_new) = lookup(xkb_library, "xkb_context_new");
  *(void **)(&p_xkb_context_unref) = lookup(xkb_library, "xkb_context_unref");
  *(void **)(&p_xkb_keymap_new_from_string) =
      lookup(xkb_library, "xkb_keymap_new_from_string");
  *(void **)(&p_xkb_keymap_unref) = lookup(xkb_library, "xkb_keymap_unref");
  *(void **)(&p_xkb_state_new) = lookup(xkb_library, "xkb_state_new");
  *(void **)(&p_xkb_state_unref) = lookup(xkb_library, "xkb_state_unref");
  *(void **)(&p_xkb_state_key_get_one_sym) =
      lookup(xkb_library, "xkb_state_key_get_one_sym");
  *(void **)(&p_xkb_state_key_get_utf8) =
      lookup(xkb_library, "xkb_state_key_get_utf8");
  *(void **)(&p_xkb_state_update_mask) =
      lookup(xkb_library, "xkb_state_update_mask");
  return p_xkb_context_new != NULL && p_xkb_state_key_get_one_sym != NULL;
}

/* ------------------------------------------------------------------------- */
/* xdg-shell, as a protocol scanner would write it                            */
/* ------------------------------------------------------------------------- */

struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
struct xdg_positioner;
struct xdg_popup;

static const struct wl_interface xdg_positioner_interface;
static const struct wl_interface xdg_surface_interface;
static const struct wl_interface xdg_toplevel_interface;
static const struct wl_interface xdg_popup_interface;
static const struct wl_interface xdg_wm_base_interface;

static const struct wl_interface *xdg_create_positioner_types[] = {
  &xdg_positioner_interface,
};
static const struct wl_interface *xdg_get_xdg_surface_types[] = {
  &xdg_surface_interface, NULL,
};
static const struct wl_message xdg_wm_base_requests[] = {
  { "destroy", "", NULL },
  { "create_positioner", "n", xdg_create_positioner_types },
  { "get_xdg_surface", "no", xdg_get_xdg_surface_types },
  { "pong", "u", NULL },
};
static const struct wl_message xdg_wm_base_events[] = {
  { "ping", "u", NULL },
};
static const struct wl_interface xdg_wm_base_interface = {
  "xdg_wm_base", 2, 4, xdg_wm_base_requests, 1, xdg_wm_base_events,
};

static const struct wl_message xdg_positioner_requests[] = {
  { "destroy", "", NULL },
  { "set_size", "ii", NULL },
  { "set_anchor_rect", "iiii", NULL },
  { "set_anchor", "u", NULL },
  { "set_gravity", "u", NULL },
  { "set_constraint_adjustment", "u", NULL },
  { "set_offset", "ii", NULL },
};
static const struct wl_interface xdg_positioner_interface = {
  "xdg_positioner", 2, 7, xdg_positioner_requests, 0, NULL,
};

static const struct wl_interface *xdg_get_toplevel_types[] = {
  &xdg_toplevel_interface,
};
static const struct wl_interface *xdg_get_popup_types[] = {
  &xdg_popup_interface, &xdg_surface_interface, &xdg_positioner_interface,
};
static const struct wl_message xdg_surface_requests[] = {
  { "destroy", "", NULL },
  { "get_toplevel", "n", xdg_get_toplevel_types },
  { "get_popup", "noo", xdg_get_popup_types },
  { "set_window_geometry", "iiii", NULL },
  { "ack_configure", "u", NULL },
};
static const struct wl_message xdg_surface_events[] = {
  { "configure", "u", NULL },
};
static const struct wl_interface xdg_surface_interface = {
  "xdg_surface", 2, 5, xdg_surface_requests, 1, xdg_surface_events,
};

static const struct wl_interface *xdg_set_parent_types[] = {
  &xdg_toplevel_interface,
};
static const struct wl_message xdg_toplevel_requests[] = {
  { "destroy", "", NULL },
  { "set_parent", "?o", xdg_set_parent_types },
  { "set_title", "s", NULL },
  { "set_app_id", "s", NULL },
  { "show_window_menu", "ouii", NULL },
  { "move", "ou", NULL },
  { "resize", "ouu", NULL },
  { "set_max_size", "ii", NULL },
  { "set_min_size", "ii", NULL },
  { "set_maximized", "", NULL },
  { "unset_maximized", "", NULL },
  { "set_fullscreen", "?o", NULL },
  { "unset_fullscreen", "", NULL },
  { "set_minimized", "", NULL },
};
static const struct wl_message xdg_toplevel_events[] = {
  { "configure", "iia", NULL },
  { "close", "", NULL },
};
static const struct wl_interface xdg_toplevel_interface = {
  "xdg_toplevel", 2, 14, xdg_toplevel_requests, 2, xdg_toplevel_events,
};

static const struct wl_message xdg_popup_requests[] = {
  { "destroy", "", NULL },
  { "grab", "ou", NULL },
};
static const struct wl_message xdg_popup_events[] = {
  { "configure", "iiii", NULL },
  { "popup_done", "", NULL },
};
static const struct wl_interface xdg_popup_interface = {
  "xdg_popup", 2, 2, xdg_popup_requests, 2, xdg_popup_events,
};

/* ------------------------------------------------------------------------- */
/* Listeners                                                                  */
/* ------------------------------------------------------------------------- */

struct wl_registry_listener_ {
  void (*global)(void *, struct wl_proxy *, uint32_t, const char *, uint32_t);
  void (*global_remove)(void *, struct wl_proxy *, uint32_t);
};

struct wl_buffer_listener_ {
  void (*release)(void *, struct wl_proxy *);
};

struct wl_seat_listener_ {
  void (*capabilities)(void *, struct wl_proxy *, uint32_t);
  void (*name)(void *, struct wl_proxy *, const char *);
};

struct wl_keyboard_listener_ {
  void (*keymap)(void *, struct wl_proxy *, uint32_t, int, uint32_t);
  void (*enter)(void *, struct wl_proxy *, uint32_t, struct wl_proxy *,
                struct wl_array *);
  void (*leave)(void *, struct wl_proxy *, uint32_t, struct wl_proxy *);
  void (*key)(void *, struct wl_proxy *, uint32_t, uint32_t, uint32_t, uint32_t);
  void (*modifiers)(void *, struct wl_proxy *, uint32_t, uint32_t, uint32_t,
                    uint32_t, uint32_t);
  void (*repeat_info)(void *, struct wl_proxy *, int32_t, int32_t);
};

struct wl_pointer_listener_ {
  void (*enter)(void *, struct wl_proxy *, uint32_t, struct wl_proxy *,
                wl_fixed_t, wl_fixed_t);
  void (*leave)(void *, struct wl_proxy *, uint32_t, struct wl_proxy *);
  void (*motion)(void *, struct wl_proxy *, uint32_t, wl_fixed_t, wl_fixed_t);
  void (*button)(void *, struct wl_proxy *, uint32_t, uint32_t, uint32_t,
                 uint32_t);
  void (*axis)(void *, struct wl_proxy *, uint32_t, uint32_t, wl_fixed_t);
  void (*frame)(void *, struct wl_proxy *);
  void (*axis_source)(void *, struct wl_proxy *, uint32_t);
  void (*axis_stop)(void *, struct wl_proxy *, uint32_t, uint32_t);
  void (*axis_discrete)(void *, struct wl_proxy *, uint32_t, int32_t);
};

struct wl_output_listener_ {
  void (*geometry)(void *, struct wl_proxy *, int32_t, int32_t, int32_t,
                   int32_t, int32_t, const char *, const char *, int32_t);
  void (*mode)(void *, struct wl_proxy *, uint32_t, int32_t, int32_t, int32_t);
  void (*done)(void *, struct wl_proxy *);
  void (*scale)(void *, struct wl_proxy *, int32_t);
};

struct xdg_wm_base_listener_ {
  void (*ping)(void *, struct wl_proxy *, uint32_t);
};

struct xdg_surface_listener_ {
  void (*configure)(void *, struct wl_proxy *, uint32_t);
};

struct xdg_toplevel_listener_ {
  void (*configure)(void *, struct wl_proxy *, int32_t, int32_t,
                    struct wl_array *);
  void (*close)(void *, struct wl_proxy *);
};

struct wl_data_offer_listener_ {
  void (*offer)(void *, struct wl_proxy *, const char *);
  void (*source_actions)(void *, struct wl_proxy *, uint32_t);
  void (*action)(void *, struct wl_proxy *, uint32_t);
};

struct wl_data_device_listener_ {
  void (*data_offer)(void *, struct wl_proxy *, struct wl_proxy *);
  void (*enter)(void *, struct wl_proxy *, uint32_t, struct wl_proxy *,
                wl_fixed_t, wl_fixed_t, struct wl_proxy *);
  void (*leave)(void *, struct wl_proxy *);
  void (*motion)(void *, struct wl_proxy *, uint32_t, wl_fixed_t, wl_fixed_t);
  void (*drop)(void *, struct wl_proxy *);
  void (*selection)(void *, struct wl_proxy *, struct wl_proxy *);
};

struct wl_data_source_listener_ {
  void (*target)(void *, struct wl_proxy *, const char *);
  void (*send)(void *, struct wl_proxy *, const char *, int32_t);
  void (*cancelled)(void *, struct wl_proxy *);
};

/* ------------------------------------------------------------------------- */
/* State                                                                      */
/* ------------------------------------------------------------------------- */

typedef struct moonapp_buffer {
  struct wl_proxy *buffer;
  void *data;
  int32_t busy;
  int32_t width;
  int32_t height;
  struct wl_listener_holder {
    void *listener[2];
  } release;
  int32_t index;
  struct moonapp_win *win;
} moonapp_buffer;

typedef struct moonapp_win {
  int32_t id;
  int32_t alive;
  struct moonapp_host *host;
  struct wl_proxy *surface;
  struct wl_proxy *xdg_surface;
  struct wl_proxy *toplevel;
  int32_t width;
  int32_t height;
  double scale;
  int32_t cursor;
  int32_t ime_allowed;
  int32_t configured;
  int32_t pending_width;
  int32_t pending_height;
  int32_t closed;
  moonapp_buffer buffers[2];
  void *listener[4];
  void *surface_listener[2];
  void *toplevel_listener[3];
} moonapp_win;

typedef struct moonapp_host {
  moonapp_queue queue;
  moonapp_event current;
  int32_t has_current;
  int32_t next_id;
  moonapp_win windows[MOONAPP_MAX_WINDOWS];
  int32_t count;
  char error[MOONAPP_ERROR];
  struct wl_display *display;
  struct wl_proxy *registry;
  struct wl_proxy *compositor;
  struct wl_proxy *shm;
  struct wl_proxy *wm_base;
  struct wl_proxy *seat;
  struct wl_proxy *keyboard;
  struct wl_proxy *pointer;
  struct wl_proxy *data_device_manager;
  struct wl_proxy *data_device;
  struct wl_proxy *data_offer;
  struct wl_proxy *data_source;
  int32_t has_keyboard;
  int32_t has_pointer;
  int32_t output_scale;
  int32_t outputs;
  void *output_listener[4][5];
  int32_t quitting;
  struct xkb_context *xkb;
  struct xkb_keymap *keymap;
  struct xkb_state *xkb_state;
  int32_t shift;
  int32_t control;
  int32_t alt;
  int32_t meta;
  int32_t caps;
  int32_t num;
  char *clipboard_text;
  int32_t clipboard_len;
  char *offered_mime;
  void *registry_listener[3];
  void *seat_listener[3];
  void *keyboard_listener[7];
  void *pointer_listener[10];
  void *wm_base_listener[2];
  void *data_device_listener[7];
  void *data_offer_listener[4];
  void *data_source_listener[4];
  struct moonapp_win *pointer_focus;
  struct moonapp_win *keyboard_focus;
} moonapp_host;

static moonapp_event *push_event(moonapp_host *host, int32_t kind,
                                 int32_t window) {
  moonapp_event *event = moonapp_queue_push(&host->queue);
  if (event == NULL) return NULL;
  event->kind = kind;
  event->window = window;
  return event;
}

static moonapp_win *find_window(moonapp_host *host, int32_t id) {
  for (int32_t i = 0; i < host->count; i++) {
    if (host->windows[i].alive && host->windows[i].id == id) {
      return &host->windows[i];
    }
  }
  return NULL;
}

static moonapp_win *window_of_surface(moonapp_host *host,
                                      struct wl_proxy *surface) {
  for (int32_t i = 0; i < host->count; i++) {
    if (host->windows[i].alive && host->windows[i].surface == surface) {
      return &host->windows[i];
    }
  }
  return NULL;
}

/* ------------------------------------------------------------------------- */
/* Frame buffers                                                              */
/* ------------------------------------------------------------------------- */

static int32_t create_anonymous_file(size_t size) {
  static const char *directories[] = { "/dev/shm", "/tmp", NULL };
  for (int32_t i = 0; directories[i] != NULL; i++) {
    char name[256];
    snprintf(name, sizeof(name), "%s/moonapp-XXXXXX", directories[i]);
    int fd = mkstemp(name);
    if (fd < 0) continue;
    unlink(name);
    if (ftruncate(fd, (off_t)size) != 0) {
      close(fd);
      continue;
    }
    return fd;
  }
  return -1;
}

static void buffer_release(void *data, struct wl_proxy *buffer) {
  (void)buffer;
  moonapp_buffer *target = (moonapp_buffer *)data;
  target->busy = 0;
}

static void release_buffer(moonapp_win *win, moonapp_buffer *buffer) {
  if (buffer->buffer != NULL) {
    p_wl_proxy_marshal_flags(buffer->buffer, 0, NULL,
                             1,
                             WL_MARSHAL_FLAG_DESTROY);
    buffer->buffer = NULL;
  }
  (void)win;
}

static int32_t ensure_buffers(moonapp_win *win, int32_t width, int32_t height) {
  moonapp_host *host = win->host;
  if (win->buffers[0].buffer != NULL && win->buffers[0].width == width &&
      win->buffers[0].height == height) {
    return 1;
  }
  for (int32_t i = 0; i < 2; i++) {
    release_buffer(win, &win->buffers[i]);
    if (win->buffers[i].data != NULL) {
      munmap(win->buffers[i].data,
             (size_t)win->buffers[i].width * (size_t)win->buffers[i].height * 4);
      win->buffers[i].data = NULL;
    }
  }
  if (width <= 0 || height <= 0) return 0;
  size_t stride = (size_t)width * 4;
  size_t one = stride * (size_t)height;
  int fd = create_anonymous_file(one * 2);
  if (fd < 0) {
    moonapp_error(host->error, "no shared memory for a frame");
    return 0;
  }
  void *mapped = mmap(NULL, one * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped == MAP_FAILED) {
    close(fd);
    moonapp_error(host->error, "no shared memory for a frame");
    return 0;
  }
  struct wl_proxy *pool = p_wl_proxy_marshal_flags(
      host->shm, 0 /* create_pool */, wl_shm_pool_interface, 1, 0, fd, (int32_t)(one * 2), NULL);
  if (pool == NULL) {
    munmap(mapped, one * 2);
    close(fd);
    return 0;
  }
  for (int32_t i = 0; i < 2; i++) {
    moonapp_buffer *buffer = &win->buffers[i];
    buffer->data = (char *)mapped + one * (size_t)i;
    buffer->width = width;
    buffer->height = height;
    buffer->busy = 0;
    buffer->win = win;
    buffer->index = i;
    buffer->buffer = p_wl_proxy_marshal_flags(
        pool, 0 /* create_buffer */, wl_buffer_interface, 1, 0, 0 /* offset */, (int32_t)width,
        (int32_t)height, (int32_t)stride, 0 /* ARGB8888 */, NULL);
    if (buffer->buffer == NULL) return 0;
    struct wl_buffer_listener_ *listener =
        (struct wl_buffer_listener_ *)buffer->release.listener;
    listener->release = buffer_release;
    p_wl_proxy_add_listener(buffer->buffer, (void (**)(void))listener, buffer);
  }
  p_wl_proxy_marshal_flags(pool, 1 /* destroy */, NULL,
                           1,
                           WL_MARSHAL_FLAG_DESTROY);
  close(fd);
  return 1;
}

static void xdg_wm_base_ping(void *data, struct wl_proxy *wm_base,
                             uint32_t serial);

/* ------------------------------------------------------------------------- */
/* Outputs, for the scale a window is laid out at                          */
/* ------------------------------------------------------------------------- */

static void output_geometry(void *data, struct wl_proxy *output, int32_t x,
                            int32_t y, int32_t physical_width,
                            int32_t physical_height, int32_t subpixel,
                            const char *make, const char *model,
                            int32_t transform) {
  (void)data; (void)output; (void)x; (void)y; (void)physical_width;
  (void)physical_height; (void)subpixel; (void)make; (void)model;
  (void)transform;
}

static void output_mode(void *data, struct wl_proxy *output, uint32_t flags,
                        int32_t width, int32_t height, int32_t refresh) {
  (void)data; (void)output; (void)flags; (void)width; (void)height;
  (void)refresh;
}

static void output_done(void *data, struct wl_proxy *output) {
  (void)data; (void)output;
}

static void output_scale_event(void *data, struct wl_proxy *output,
                               int32_t factor) {
  (void)output;
  int32_t *scale = (int32_t *)data;
  if (factor > *scale) *scale = factor;
}

/* ------------------------------------------------------------------------- */
/* Registry                                                                   */
/* ------------------------------------------------------------------------- */

static void registry_global(void *data, struct wl_proxy *registry, uint32_t name,
                            const char *interface, uint32_t version) {
  moonapp_host *host = (moonapp_host *)data;
  if (strcmp(interface, "wl_compositor") == 0 && host->compositor == NULL) {
    host->compositor = p_wl_proxy_marshal_flags(
        registry, 0 /* bind */, wl_compositor_interface, version <= 4 ? version : 4,
        0, name, interface, version <= 4 ? version : 4, NULL);
  } else if (strcmp(interface, "wl_shm") == 0 && host->shm == NULL) {
    host->shm = p_wl_proxy_marshal_flags(registry, 0, wl_shm_interface, 1, 0,
                                         name, interface, 1, NULL);
  } else if (strcmp(interface, "xdg_wm_base") == 0 && host->wm_base == NULL) {
    host->wm_base = p_wl_proxy_marshal_flags(
        registry, 0, &xdg_wm_base_interface,
        version < 2 ? version : 2, 0, name, interface, version < 2 ? version : 2,
        NULL);
    if (host->wm_base != NULL) {
      struct xdg_wm_base_listener_ *listener =
          (struct xdg_wm_base_listener_ *)host->wm_base_listener;
      listener->ping = xdg_wm_base_ping;
      p_wl_proxy_add_listener(host->wm_base, (void (**)(void))listener, host);
    }
  } else if (strcmp(interface, "wl_seat") == 0 && host->seat == NULL) {
    uint32_t seat_version = version < 5 ? version : 5;
    host->seat = p_wl_proxy_marshal_flags(registry, 0, wl_seat_interface,
                                          seat_version, 0, name, interface,
                                          seat_version, NULL);
    if (host->seat != NULL) {
      struct wl_seat_listener_ *listener =
          (struct wl_seat_listener_ *)host->seat_listener;
      p_wl_proxy_add_listener(host->seat, (void (**)(void))listener, host);
    }
  } else if (strcmp(interface, "wl_output") == 0 && host->outputs < 4) {
    uint32_t output_version = version < 2 ? version : 2;
    struct wl_proxy *output = p_wl_proxy_marshal_flags(
        registry, 0, wl_output_interface, output_version, 0, name, interface,
        output_version, NULL);
    if (output != NULL) {
      struct wl_output_listener_ *listener =
          (struct wl_output_listener_ *)host->output_listener[host->outputs];
      listener->geometry = output_geometry;
      listener->mode = output_mode;
      listener->done = output_done;
      listener->scale = output_scale_event;
      p_wl_proxy_add_listener(output, (void (**)(void))listener,
                              &host->output_scale);
      host->outputs++;
    }
  } else if (strcmp(interface, "wl_data_device_manager") == 0 &&
             host->data_device_manager == NULL) {
    uint32_t manager_version = version < 3 ? version : 3;
    host->data_device_manager = p_wl_proxy_marshal_flags(
        registry, 0, wl_data_device_manager_interface, manager_version, 0, name,
        interface, manager_version, NULL);
  }
}

static void registry_remove(void *data, struct wl_proxy *registry,
                            uint32_t name) {
  (void)data;
  (void)registry;
  (void)name;
}

/* The seat brings the two devices this library reads from. */
static void seat_capabilities(void *data, struct wl_proxy *seat,
                              uint32_t capabilities) {
  moonapp_host *host = (moonapp_host *)data;
  if ((capabilities & 2 /* pointer */) && host->pointer == NULL) {
    host->pointer = p_wl_proxy_marshal_flags(
        seat, 0 /* get_pointer */, wl_pointer_interface, 5, 0, NULL);
    host->has_pointer = host->pointer != NULL;
    if (host->pointer != NULL) {
      p_wl_proxy_add_listener(host->pointer,
                              (void (**)(void))host->pointer_listener, host);
    }
  }
  if ((capabilities & 1 /* keyboard */) && host->keyboard == NULL) {
    host->keyboard = p_wl_proxy_marshal_flags(
        seat, 1 /* get_keyboard */, wl_keyboard_interface, 5, 0, NULL);
    host->has_keyboard = host->keyboard != NULL;
    if (host->keyboard != NULL) {
      p_wl_proxy_add_listener(host->keyboard,
                              (void (**)(void))host->keyboard_listener, host);
    }
  }
}

static void seat_name(void *data, struct wl_proxy *seat, const char *name) {
  (void)data;
  (void)seat;
  (void)name;
}

/* ------------------------------------------------------------------------- */
/* Keyboard                                                                   */
/* ------------------------------------------------------------------------- */

static void keyboard_keymap(void *data, struct wl_proxy *keyboard,
                            uint32_t format, int fd, uint32_t size) {
  (void)keyboard;
  moonapp_host *host = (moonapp_host *)data;
  if (format != 1 /* XKB_V1 */) {
    close(fd);
    return;
  }
  char *map = (char *)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (map == MAP_FAILED) {
    close(fd);
    return;
  }
  if (load_xkb()) {
    if (host->xkb == NULL) host->xkb = p_xkb_context_new(0);
    if (host->keymap != NULL) p_xkb_keymap_unref(host->keymap);
    if (host->xkb_state != NULL) p_xkb_state_unref(host->xkb_state);
    /* A keymap string ends at the first NUL the compositor left in the map. */
    host->keymap = p_xkb_keymap_new_from_string(host->xkb, map, 1, 0);
    host->xkb_state = host->keymap != NULL ? p_xkb_state_new(host->keymap) : NULL;
  }
  munmap(map, size);
  close(fd);
}

static void keyboard_enter(void *data, struct wl_proxy *keyboard,
                           uint32_t serial, struct wl_proxy *surface,
                           struct wl_array *keys) {
  (void)keyboard;
  (void)serial;
  (void)keys;
  moonapp_host *host = data;
  moonapp_win *win = window_of_surface(host, surface);
  host->keyboard_focus = win;
  if (win != NULL) {
    moonapp_event *focused = push_event(host, MOONAPP_EV_FOCUSED, win->id);
    if (focused != NULL) focused->i32a = 1;
  }
}

static void keyboard_leave(void *data, struct wl_proxy *keyboard,
                           uint32_t serial, struct wl_proxy *surface) {
  (void)keyboard;
  (void)serial;
  moonapp_host *host = data;
  moonapp_win *win = window_of_surface(host, surface);
  host->keyboard_focus = NULL;
  if (win != NULL) {
    moonapp_event *focused = push_event(host, MOONAPP_EV_FOCUSED, win->id);
    if (focused != NULL) focused->i32a = 0;
  }
}

static void keyboard_modifiers(void *data, struct wl_proxy *keyboard,
                               uint32_t serial, uint32_t depressed,
                               uint32_t latched, uint32_t locked,
                               uint32_t group) {
  (void)keyboard;
  (void)serial;
  (void)group;
  moonapp_host *host = data;
  if (host->xkb_state != NULL && p_xkb_state_update_mask != NULL) {
    p_xkb_state_update_mask(host->xkb_state, depressed, latched, locked, 0,
                            0, 0);
  }
}

static int32_t modifier_bits(moonapp_host *host) {
  int32_t bits = 0;
  if (host->shift) bits |= 1;
  if (host->control) bits |= 2;
  if (host->alt) bits |= 4;
  if (host->meta) bits |= 8;
  if (host->caps) bits |= 16;
  if (host->num) bits |= 32;
  return bits;
}

static void keyboard_key(void *data, struct wl_proxy *keyboard, uint32_t serial,
                         uint32_t time, uint32_t key, uint32_t state) {
  (void)keyboard;
  (void)serial;
  (void)time;
  moonapp_host *host = data;
  moonapp_win *win = host->keyboard_focus;
  if (win == NULL) return;
  /* Wayland counts from eight, the way the kernel does. */
  uint32_t code = key + 8;
  uint32_t keysym = 0;
  if (host->xkb_state != NULL) {
    keysym = p_xkb_state_key_get_one_sym(host->xkb_state, code);
  } else {
    keysym = 0x00 + code;
  }
  int32_t down = state == 1;
  moonapp_event *event = push_event(
      host, down ? MOONAPP_EV_KEY_DOWN : MOONAPP_EV_KEY_UP, win->id);
  if (event != NULL) {
    event->i32a = (int32_t)keysym;
    event->i32b = modifier_bits(host);
    event->i32c = 0;
    event->i32d = (int32_t)code;
  }
  /* The modifier bits are tracked from the keys themselves: the mask the
   * compositor sends is a layout index, not a name. */
  switch (keysym) {
    case 0xFFE1:
    case 0xFFE2: host->shift = down; break;
    case 0xFFE3:
    case 0xFFE4: host->control = down; break;
    case 0xFFE9:
    case 0xFFEA: host->alt = down; break;
    case 0xFFEB:
    case 0xFFEC:
    case 0xFFE7:
    case 0xFFE8: host->meta = down; break;
    case 0xFFE5:
      if (down) host->caps = !host->caps;
      break;
    case 0xFF7F:
      if (down) host->num = !host->num;
      break;
    default: break;
  }
  if (down && host->xkb_state != NULL) {
    char text[16];
    int len = p_xkb_state_key_get_utf8(host->xkb_state, code, text,
                                       sizeof(text));
    if (len > 0 && (unsigned char)text[0] >= 0x20 &&
        (unsigned char)text[0] != 0x7F) {
      moonapp_event *typed = push_event(host, MOONAPP_EV_TEXT, win->id);
      if (typed != NULL) moonapp_event_set_text(typed, text, len);
    }
  }
}

static void keyboard_repeat(void *data, struct wl_proxy *keyboard,
                            int32_t rate, int32_t delay) {
  (void)data;
  (void)keyboard;
  (void)rate;
  (void)delay;
}

/* ------------------------------------------------------------------------- */
/* Pointer                                                                    */
/* ------------------------------------------------------------------------- */

static void pointer_enter(void *data, struct wl_proxy *pointer, uint32_t serial,
                          struct wl_proxy *surface, wl_fixed_t x,
                          wl_fixed_t y) {
  (void)pointer;
  (void)serial;
  moonapp_host *host = data;
  moonapp_win *win = window_of_surface(host, surface);
  host->pointer_focus = win;
  if (win != NULL) {
    push_event(host, MOONAPP_EV_POINTER_ENTERED, win->id);
    moonapp_event *moved = push_event(host, MOONAPP_EV_POINTER_MOVED, win->id);
    if (moved != NULL) {
      moved->f64a = (double)x / 256.0;
      moved->f64b = (double)y / 256.0;
    }
  }
}

static void pointer_leave(void *data, struct wl_proxy *pointer, uint32_t serial,
                          struct wl_proxy *surface) {
  (void)pointer;
  (void)serial;
  moonapp_host *host = data;
  moonapp_win *win = window_of_surface(host, surface);
  host->pointer_focus = NULL;
  if (win != NULL) push_event(host, MOONAPP_EV_POINTER_LEFT, win->id);
}

static void pointer_motion(void *data, struct wl_proxy *pointer, uint32_t time,
                           wl_fixed_t x, wl_fixed_t y) {
  (void)pointer;
  (void)time;
  moonapp_host *host = data;
  moonapp_win *win = host->pointer_focus;
  if (win == NULL) return;
  moonapp_event *moved = push_event(host, MOONAPP_EV_POINTER_MOVED, win->id);
  if (moved != NULL) {
    moved->f64a = (double)x / 256.0;
    moved->f64b = (double)y / 256.0;
  }
}

static void pointer_button(void *data, struct wl_proxy *pointer,
                           uint32_t serial, uint32_t time, uint32_t button,
                           uint32_t state) {
  (void)pointer;
  (void)serial;
  (void)time;
  moonapp_host *host = data;
  moonapp_win *win = host->pointer_focus;
  if (win == NULL) return;
  int32_t code = 5 + (int32_t)button;
  switch (button) {
    case 0x110: code = 0; break;
    case 0x111: code = 1; break;
    case 0x112: code = 2; break;
    case 0x113: code = 3; break;
    case 0x114: code = 4; break;
    default: break;
  }
  moonapp_event *event = push_event(
      host, state == 1 ? MOONAPP_EV_BUTTON_DOWN : MOONAPP_EV_BUTTON_UP, win->id);
  if (event != NULL) {
    event->i32a = code;
    event->i32b = modifier_bits(host);
  }
}

static void pointer_axis(void *data, struct wl_proxy *pointer, uint32_t time,
                         uint32_t axis, wl_fixed_t value) {
  (void)pointer;
  (void)time;
  moonapp_host *host = data;
  moonapp_win *win = host->pointer_focus;
  if (win == NULL) return;
  moonapp_event *event = push_event(host, MOONAPP_EV_WHEEL, win->id);
  if (event != NULL) {
    double amount = (double)value / 256.0;
    event->i32a = 1; /* pixels: a Wayland axis is a distance, not a notch */
    if (axis == 0) {
      event->f64b = -amount;
    } else {
      event->f64a = amount;
    }
  }
}

static void pointer_frame(void *data, struct wl_proxy *pointer) {
  (void)data;
  (void)pointer;
}

static void pointer_axis_source(void *data, struct wl_proxy *pointer,
                                uint32_t source) {
  (void)data;
  (void)pointer;
  (void)source;
}

static void pointer_axis_stop(void *data, struct wl_proxy *pointer,
                              uint32_t time, uint32_t axis) {
  (void)data;
  (void)pointer;
  (void)time;
  (void)axis;
}

static void pointer_axis_discrete(void *data, struct wl_proxy *pointer,
                                  uint32_t axis, int32_t discrete) {
  (void)data;
  (void)pointer;
  (void)axis;
  (void)discrete;
}

/* ------------------------------------------------------------------------- */
/* Clipboard                                                                  */
/* ------------------------------------------------------------------------- */

static void data_offer_offer(void *data, struct wl_proxy *offer,
                             const char *mime) {
  (void)offer;
  moonapp_host *host = data;
  if (strcmp(mime, "text/plain;charset=utf-8") == 0 ||
      strcmp(mime, "UTF8_STRING") == 0 || strcmp(mime, "text/plain") == 0) {
    if (host->offered_mime == NULL) {
      host->offered_mime = strdup(mime);
    }
  }
}

static void data_offer_source_actions(void *data, struct wl_proxy *offer,
                                      uint32_t actions) {
  (void)data;
  (void)offer;
  (void)actions;
}

static void data_offer_action(void *data, struct wl_proxy *offer,
                              uint32_t action) {
  (void)data;
  (void)offer;
  (void)action;
}

static void data_device_data_offer(void *data, struct wl_proxy *device,
                                   struct wl_proxy *offer) {
  (void)device;
  moonapp_host *host = data;
  host->data_offer = offer;
  free(host->offered_mime);
  host->offered_mime = NULL;
  struct wl_data_offer_listener_ *listener =
      (struct wl_data_offer_listener_ *)host->data_offer_listener;
  listener->offer = data_offer_offer;
  listener->source_actions = data_offer_source_actions;
  listener->action = data_offer_action;
  p_wl_proxy_add_listener(offer, (void (**)(void))listener, host);
}

static void data_device_enter(void *data, struct wl_proxy *device,
                              uint32_t serial, struct wl_proxy *surface,
                              wl_fixed_t x, wl_fixed_t y,
                              struct wl_proxy *offer) {
  (void)data;
  (void)device;
  (void)serial;
  (void)surface;
  (void)x;
  (void)y;
  (void)offer;
}

static void data_device_leave(void *data, struct wl_proxy *device) {
  (void)data;
  (void)device;
}

static void data_device_motion(void *data, struct wl_proxy *device,
                               uint32_t time, wl_fixed_t x, wl_fixed_t y) {
  (void)data;
  (void)device;
  (void)time;
  (void)x;
  (void)y;
}

static void data_device_drop(void *data, struct wl_proxy *device) {
  (void)data;
  (void)device;
}

static void data_device_selection(void *data, struct wl_proxy *device,
                                  struct wl_proxy *offer) {
  (void)device;
  moonapp_host *host = data;
  if (offer == NULL) {
    free(host->offered_mime);
    host->offered_mime = NULL;
  }
}

static void data_source_target(void *data, struct wl_proxy *source,
                               const char *mime) {
  (void)data;
  (void)source;
  (void)mime;
}

static void data_source_send(void *data, struct wl_proxy *source,
                             const char *mime, int32_t fd) {
  (void)source;
  (void)mime;
  moonapp_host *host = data;
  if (host->clipboard_text != NULL && host->clipboard_len > 0) {
    ssize_t written = 0;
    while (written < host->clipboard_len) {
      ssize_t step = write(fd, host->clipboard_text + written,
                           (size_t)(host->clipboard_len - written));
      if (step <= 0) break;
      written += step;
    }
  }
  close(fd);
}

static void data_source_cancelled(void *data, struct wl_proxy *source) {
  (void)data;
  if (source != NULL) {
    p_wl_proxy_marshal_flags(source, 1 /* destroy */, NULL,
                             1,
                             WL_MARSHAL_FLAG_DESTROY);
  }
}

/* ------------------------------------------------------------------------- */
/* Windows                                                                    */
/* ------------------------------------------------------------------------- */

static void xdg_wm_base_ping(void *data, struct wl_proxy *wm_base,
                             uint32_t serial) {
  (void)data;
  p_wl_proxy_marshal_flags(wm_base, 3 /* pong */, NULL,
                           1, 0, serial);
}

static void surface_configure(void *data, struct wl_proxy *surface,
                              uint32_t serial) {
  moonapp_win *win = data;
  moonapp_host *host = win->host;
  (void)surface;
  p_wl_proxy_marshal_flags(win->xdg_surface, 4 /* ack_configure */, NULL,
                           1, 0, serial);
  win->configured = 1;
  if (win->pending_width > 0 && win->pending_height > 0 &&
      (win->pending_width != win->width || win->pending_height != win->height)) {
    win->width = win->pending_width;
    win->height = win->pending_height;
    moonapp_event *resized = push_event(host, MOONAPP_EV_RESIZED, win->id);
    if (resized != NULL) {
      resized->i32a = win->width;
      resized->i32b = win->height;
      resized->f64a = win->scale;
    }
  }
  p_wl_display_flush(host->display);
}

static void toplevel_configure(void *data, struct wl_proxy *toplevel, int32_t width,
                               int32_t height, struct wl_array *states) {
  (void)toplevel;
  (void)states;
  moonapp_win *win = data;
  if (width > 0) win->pending_width = width;
  if (height > 0) win->pending_height = height;
}

static void toplevel_close(void *data, struct wl_proxy *toplevel) {
  (void)toplevel;
  moonapp_win *win = data;
  win->closed = 1;
  push_event(win->host, MOONAPP_EV_CLOSE, win->id);
}

/* ------------------------------------------------------------------------- */
/* Dispatch                                                                   */
/* ------------------------------------------------------------------------- */

static void fill_listeners(moonapp_host *host) {
  struct wl_registry_listener_ *registry =
      (struct wl_registry_listener_ *)host->registry_listener;
  registry->global = registry_global;
  registry->global_remove = registry_remove;
  struct wl_seat_listener_ *seat = (struct wl_seat_listener_ *)host->seat_listener;
  seat->capabilities = seat_capabilities;
  seat->name = seat_name;
  struct wl_keyboard_listener_ *keyboard =
      (struct wl_keyboard_listener_ *)host->keyboard_listener;
  keyboard->keymap = keyboard_keymap;
  keyboard->enter = keyboard_enter;
  keyboard->leave = keyboard_leave;
  keyboard->key = keyboard_key;
  keyboard->modifiers = keyboard_modifiers;
  keyboard->repeat_info = keyboard_repeat;
  struct wl_pointer_listener_ *pointer =
      (struct wl_pointer_listener_ *)host->pointer_listener;
  pointer->enter = pointer_enter;
  pointer->leave = pointer_leave;
  pointer->motion = pointer_motion;
  pointer->button = pointer_button;
  pointer->axis = pointer_axis;
  pointer->frame = pointer_frame;
  pointer->axis_source = pointer_axis_source;
  pointer->axis_stop = pointer_axis_stop;
  pointer->axis_discrete = pointer_axis_discrete;
  struct wl_data_device_listener_ *device =
      (struct wl_data_device_listener_ *)host->data_device_listener;
  device->data_offer = data_device_data_offer;
  device->enter = data_device_enter;
  device->leave = data_device_leave;
  device->motion = data_device_motion;
  device->drop = data_device_drop;
  device->selection = data_device_selection;
  struct wl_data_source_listener_ *source =
      (struct wl_data_source_listener_ *)host->data_source_listener;
  source->target = data_source_target;
  source->send = data_source_send;
  source->cancelled = data_source_cancelled;
}

/* Ask the library to read the socket, wait at most `timeout_ms` for something to
 * arrive, and dispatch what did. Zero reads without waiting; a negative timeout
 * waits until there is something. */
static int32_t read_socket(moonapp_host *host, int32_t timeout_ms) {
  if (p_wl_display_flush(host->display) < 0 && errno != EAGAIN) return -1;
  if (p_wl_display_prepare_read(host->display) != 0) {
    /* Something is already queued inside the library. */
    return p_wl_display_dispatch_pending(host->display);
  }
  struct pollfd socket;
  socket.fd = p_wl_display_get_fd(host->display);
  socket.events = POLLIN;
  socket.revents = 0;
  int32_t ready = poll(&socket, 1, timeout_ms < 0 ? -1 : timeout_ms);
  if (ready <= 0) {
    p_wl_display_cancel_read(host->display);
    return 0;
  }
  if (p_wl_display_read_events(host->display) < 0) return -1;
  return p_wl_display_dispatch_pending(host->display);
}

/* ------------------------------------------------------------------------- */
/* Host entry points                                                          */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
moonapp_host *wl_host_new(void) {
  if (!load_wayland()) return NULL;
  struct wl_display *display = p_wl_display_connect(NULL);
  if (display == NULL) return NULL;
  moonapp_host *host = (moonapp_host *)calloc(1, sizeof(moonapp_host));
  if (host == NULL) {
    p_wl_display_disconnect(display);
    return NULL;
  }
  moonapp_queue_init(&host->queue);
  host->next_id = 1;
  host->display = display;
  host->registry = p_wl_proxy_marshal_flags(
      (struct wl_proxy *)display, 1 /* get_registry */, wl_registry_interface, 1,
      0, NULL);
  fill_listeners(host);
  p_wl_proxy_add_listener(host->registry,
                          (void (**)(void))host->registry_listener, host);
  p_wl_display_roundtrip(display);
  p_wl_display_roundtrip(display);
  if (host->compositor == NULL || host->shm == NULL || host->wm_base == NULL) {
    moonapp_error(host->error, "the compositor has no xdg-shell or no shared memory");
    p_wl_display_disconnect(display);
    free(host);
    return NULL;
  }
  if (0) {
    host->data_device = p_wl_proxy_marshal_flags(
        host->data_device_manager, 1 /* get_data_device */,
        wl_data_device_interface, 3, 0, host->seat, NULL);
    if (host->data_device != NULL) {
      struct wl_data_device_listener_ *device =
          (struct wl_data_device_listener_ *)host->data_device_listener;
      p_wl_proxy_add_listener(host->data_device, (void (**)(void))device, host);
    }
  }
  p_wl_display_roundtrip(display);
  return host;
}

MOONBIT_FFI_EXPORT
int32_t wl_host_ok(moonapp_host *host) { return host != NULL ? 1 : 0; }

MOONBIT_FFI_EXPORT
int32_t wl_host_error(moonapp_host *host, char *into, int32_t cap) {
  const char *message =
      host == NULL ? "no compositor to connect to" : host->error;
  if (into == NULL || cap <= 0) return 0;
  int32_t len = (int32_t)strlen(message);
  if (len > cap - 1) len = cap - 1;
  memcpy(into, message, (size_t)len);
  into[len] = 0;
  return len;
}

MOONBIT_FFI_EXPORT
void wl_host_free(moonapp_host *host) {
  if (host == NULL) return;
  for (int32_t i = 0; i < host->count; i++) {
    if (!host->windows[i].alive) continue;
    moonapp_win *win = &host->windows[i];
    for (int32_t b = 0; b < 2; b++) {
      release_buffer(win, &win->buffers[b]);
      if (win->buffers[b].data != NULL) {
        munmap(win->buffers[b].data,
               (size_t)win->buffers[b].width * (size_t)win->buffers[b].height * 4);
      }
    }
    if (win->toplevel != NULL) {
      p_wl_proxy_marshal_flags(win->toplevel, 0, NULL,
                               1,
                               WL_MARSHAL_FLAG_DESTROY);
    }
    if (win->xdg_surface != NULL) {
      p_wl_proxy_marshal_flags(win->xdg_surface, 0, NULL,
                               1,
                               WL_MARSHAL_FLAG_DESTROY);
    }
    if (win->surface != NULL) {
      p_wl_proxy_marshal_flags(win->surface, 0, NULL,
                               1,
                               WL_MARSHAL_FLAG_DESTROY);
    }
    win->alive = 0;
  }
  if (host->data_source != NULL) {
    p_wl_proxy_marshal_flags(host->data_source, 1 /* destroy */, NULL,
                             1,
                             WL_MARSHAL_FLAG_DESTROY);
  }
  free(host->clipboard_text);
  free(host->offered_mime);
  if (host->keymap != NULL) p_xkb_keymap_unref(host->keymap);
  if (host->xkb_state != NULL) p_xkb_state_unref(host->xkb_state);
  if (host->xkb != NULL) p_xkb_context_unref(host->xkb);
  p_wl_display_flush(host->display);
  p_wl_display_disconnect(host->display);
  free(host);
}

MOONBIT_FFI_EXPORT
double wl_primary_scale(moonapp_host *host) {
  if (host == NULL) return 1.0;
  return host->output_scale > 0 ? (double)host->output_scale : 1.0;
}

MOONBIT_FFI_EXPORT
int32_t wl_is_shutdown(moonapp_host *host) {
  return host == NULL ? 1 : host->quitting;
}

MOONBIT_FFI_EXPORT
void wl_host_shutdown(moonapp_host *host) {
  if (host != NULL) host->quitting = 1;
}

MOONBIT_FFI_EXPORT
int32_t wl_open(moonapp_host *host, const char *title, int32_t title_len,
                     const char *app_id, int32_t app_id_len, int32_t width,
                     int32_t height, int32_t flags) {
  if (host == NULL) return -1;
  if (host->count >= MOONAPP_MAX_WINDOWS) {
    moonapp_error(host->error, "no room for another window");
    return -1;
  }
  moonapp_win *win = &host->windows[host->count];
  memset(win, 0, sizeof(*win));
  win->host = host;
  win->id = host->next_id++;
  win->alive = 1;
  win->width = width > 0 ? width : 1;
  win->height = height > 0 ? height : 1;
  win->scale = wl_primary_scale(host);
  win->cursor = 0;
  win->ime_allowed = 1;
  win->surface = p_wl_proxy_marshal_flags(
      host->compositor, 0 /* create_surface */, wl_surface_interface,
      4, 0, NULL);
  if (win->surface == NULL) {
    win->alive = 0;
    return -1;
  }
  struct xdg_surface_listener_ *surface_listener =
      (struct xdg_surface_listener_ *)win->surface_listener;
  surface_listener->configure = surface_configure;
  struct xdg_toplevel_listener_ *toplevel_listener =
      (struct xdg_toplevel_listener_ *)win->toplevel_listener;
  toplevel_listener->configure = toplevel_configure;
  toplevel_listener->close = toplevel_close;
  win->xdg_surface = p_wl_proxy_marshal_flags(
      host->wm_base, 2 /* get_xdg_surface */, &xdg_surface_interface, 2, 0, win->surface);
  if (win->xdg_surface == NULL) {
    win->alive = 0;
    return -1;
  }
  p_wl_proxy_add_listener(win->xdg_surface, (void (**)(void))surface_listener,
                          win);
  win->toplevel = p_wl_proxy_marshal_flags(
      win->xdg_surface, 1 /* get_toplevel */, &xdg_toplevel_interface, 2, 0, NULL);
  if (win->toplevel == NULL) {
    win->alive = 0;
    return -1;
  }
  p_wl_proxy_add_listener(win->toplevel, (void (**)(void))toplevel_listener, win);
  char *title_text = (char *)malloc((size_t)title_len + 1);
  if (title_text != NULL) {
    memcpy(title_text, title, (size_t)title_len);
    title_text[title_len] = 0;
    p_wl_proxy_marshal_flags(win->toplevel, 2 /* set_title */, NULL,
                             1, 0, title_text);
    free(title_text);
  }
  char *app_text = (char *)malloc((size_t)app_id_len + 1);
  if (app_text != NULL) {
    memcpy(app_text, app_id, (size_t)app_id_len);
    app_text[app_id_len] = 0;
    p_wl_proxy_marshal_flags(win->toplevel, 3 /* set_app_id */, NULL,
                             1, 0, app_text);
    free(app_text);
  }
  if (!(flags & MOONAPP_FLAG_RESIZABLE)) {
    p_wl_proxy_marshal_flags(win->toplevel, 7 /* set_max_size */, NULL,
                             1, 0, win->width,
                             win->height);
    p_wl_proxy_marshal_flags(win->toplevel, 8 /* set_min_size */, NULL,
                             1, 0, win->width,
                             win->height);
  }
  host->count++;
  /* A surface has nothing to show until it has a buffer, but committing once
   * without one is how a surface is asked for its first configure. */
  p_wl_proxy_marshal_flags(win->surface, 6 /* commit */, NULL,
                           1, 0);
  p_wl_display_flush(host->display);
  /* The first configure arrives while this waits for it. */
  for (int32_t attempt = 0; attempt < 100 && !win->configured; attempt++) {
    read_socket(host, 20);
  }
  return win->id;
}

MOONBIT_FFI_EXPORT
void wl_close(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL) return;
  for (int32_t b = 0; b < 2; b++) {
    release_buffer(win, &win->buffers[b]);
    if (win->buffers[b].data != NULL) {
      munmap(win->buffers[b].data,
             (size_t)win->buffers[b].width * (size_t)win->buffers[b].height * 4);
      win->buffers[b].data = NULL;
    }
  }
  if (win->toplevel != NULL) {
    p_wl_proxy_marshal_flags(win->toplevel, 0, NULL,
                             1,
                             WL_MARSHAL_FLAG_DESTROY);
    win->toplevel = NULL;
  }
  if (win->xdg_surface != NULL) {
    p_wl_proxy_marshal_flags(win->xdg_surface, 0, NULL,
                             1,
                             WL_MARSHAL_FLAG_DESTROY);
    win->xdg_surface = NULL;
  }
  if (win->surface != NULL) {
    p_wl_proxy_marshal_flags(win->surface, 0, NULL,
                             1,
                             WL_MARSHAL_FLAG_DESTROY);
    win->surface = NULL;
  }
  win->alive = 0;
  p_wl_display_flush(host->display);
}

MOONBIT_FFI_EXPORT
int32_t wl_window_open(moonapp_host *host, int32_t id) {
  return find_window(host, id) != NULL;
}

static int32_t pump(moonapp_host *host, int32_t timeout_ms, int32_t wait) {
  if (host->has_current) return 1;
  if (moonapp_queue_pop(&host->queue, &host->current)) {
    host->has_current = 1;
    return 1;
  }
  if (read_socket(host, wait ? timeout_ms : 0) < 0) host->quitting = 1;
  if (moonapp_queue_pop(&host->queue, &host->current)) {
    host->has_current = 1;
    return 1;
  }
  return 0;
}

MOONBIT_FFI_EXPORT
int32_t wl_poll(moonapp_host *host) {
  if (host == NULL) return 0;
  return pump(host, 0, 0);
}

MOONBIT_FFI_EXPORT
int32_t wl_wait(moonapp_host *host, int32_t timeout_ms) {
  if (host == NULL) return 0;
  return pump(host, timeout_ms, 1);
}

MOONBIT_FFI_EXPORT
void wl_event_release(moonapp_host *host) {
  if (host != NULL) host->has_current = 0;
}

MOONBIT_FFI_EXPORT
int32_t wl_event_kind(moonapp_host *host) { return host->current.kind; }

MOONBIT_FFI_EXPORT
int32_t wl_event_window(moonapp_host *host) { return host->current.window; }

MOONBIT_FFI_EXPORT
int32_t wl_event_i32(moonapp_host *host, int32_t which) {
  switch (which) {
    case 0: return host->current.i32a;
    case 1: return host->current.i32b;
    case 2: return host->current.i32c;
    default: return host->current.i32d;
  }
}

MOONBIT_FFI_EXPORT
double wl_event_f64(moonapp_host *host, int32_t which) {
  return which == 0 ? host->current.f64a : host->current.f64b;
}

MOONBIT_FFI_EXPORT
int32_t wl_event_text_len(moonapp_host *host) {
  return host->current.text_len;
}

MOONBIT_FFI_EXPORT
int32_t wl_event_text(moonapp_host *host, uint8_t *buffer, int32_t cap) {
  int32_t len = host->current.text_len;
  if (buffer == NULL || cap < len) return len;
  memcpy(buffer, host->current.text, (size_t)len);
  return len;
}

MOONBIT_FFI_EXPORT
void wl_set_title(moonapp_host *host, int32_t id, const char *title,
                       int32_t len) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL || win->toplevel == NULL) return;
  char *text = (char *)malloc((size_t)len + 1);
  if (text == NULL) return;
  memcpy(text, title, (size_t)len);
  text[len] = 0;
  p_wl_proxy_marshal_flags(win->toplevel, 2, NULL,
                           1, 0, text);
  free(text);
  p_wl_display_flush(host->display);
}

MOONBIT_FFI_EXPORT
void wl_set_size(moonapp_host *host, int32_t id, int32_t width,
                      int32_t height) {
  /* A client cannot place its own window under Wayland: the size a window may
   * have is the compositor's business, and it answers with a configure. */
  moonapp_win *win = find_window(host, id);
  if (win == NULL) return;
  win->width = width > 0 ? width : 1;
  win->height = height > 0 ? height : 1;
}

MOONBIT_FFI_EXPORT
void wl_set_min_size(moonapp_host *host, int32_t id, int32_t width,
                          int32_t height) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL || win->toplevel == NULL) return;
  p_wl_proxy_marshal_flags(win->toplevel, 8 /* set_min_size */, NULL,
                           1, 0, width,
                           height);
  p_wl_display_flush(host->display);
}

MOONBIT_FFI_EXPORT
int32_t wl_width(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  return win == NULL ? 0 : win->width;
}

MOONBIT_FFI_EXPORT
int32_t wl_height(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  return win == NULL ? 0 : win->height;
}

MOONBIT_FFI_EXPORT
double wl_scale(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  return win == NULL ? 1.0 : win->scale;
}

MOONBIT_FFI_EXPORT
void wl_set_cursor(moonapp_host *host, int32_t id, int32_t cursor) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL) return;
  /* The shape of the pointer belongs to a protocol of its own
   * (cursor-shape-v1), which not every compositor has; until it is spoken here
   * the pointer keeps the shape the compositor gave it. */
  win->cursor = cursor;
}

MOONBIT_FFI_EXPORT
int32_t wl_present(moonapp_host *host, int32_t id, uint8_t *bgra,
                        int32_t width, int32_t height, int32_t stride) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL || bgra == NULL || width <= 0 || height <= 0) return 0;
  if (!ensure_buffers(win, width, height)) return 0;
  moonapp_buffer *buffer = NULL;
  for (int32_t i = 0; i < 2; i++) {
    if (!win->buffers[i].busy) {
      buffer = &win->buffers[i];
      break;
    }
  }
  if (buffer == NULL) {
    /* Both buffers are still with the compositor; one dispatch is enough for
     * the release of the oldest to come back. */
    read_socket(host, 0);
    for (int32_t i = 0; i < 2; i++) {
      if (!win->buffers[i].busy) {
        buffer = &win->buffers[i];
        break;
      }
    }
  }
  if (buffer == NULL) return 0;
  /* The pixel format is ARGB8888, which on a little-endian machine is the same
   * byte order as the buffer a desktop wants: blue first. */
  size_t row = (size_t)width * 4;
  for (int32_t y = 0; y < height; y++) {
    memcpy((char *)buffer->data + (size_t)y * row,
           bgra + (size_t)y * (size_t)stride, row);
  }
  buffer->busy = 1;
  p_wl_proxy_marshal_flags(win->surface, 1 /* attach */, NULL,
                           1, 0,
                           buffer->buffer, 0, 0);
  p_wl_proxy_marshal_flags(win->surface, 9 /* damage_buffer */, NULL,
                           1, 0, 0, 0, width,
                           height);
  p_wl_proxy_marshal_flags(win->surface, 6 /* commit */, NULL,
                           1, 0);
  p_wl_display_flush(host->display);
  return 1;
}

MOONBIT_FFI_EXPORT
int32_t wl_read_back(moonapp_host *host, int32_t id, uint8_t *out,
                          int32_t cap) {
  /* Wayland gives a client no way to read back what it has put on a surface,
   * and no protocol has been written that would: the pixels belong to the
   * compositor once they are committed. */
  (void)host;
  (void)id;
  (void)out;
  (void)cap;
  return -1;
}

/* ------------------------------------------------------------------------- */
/* Clipboard                                                                  */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
int32_t wl_clipboard_get(moonapp_host *host, uint8_t *buffer, int32_t cap) {
  if (host == NULL) return -1;
  if (host->offered_mime == NULL || host->data_offer == NULL) {
    /* Nothing offered by another client; our own text is all there is. */
    if (host->clipboard_len <= 0) return -1;
    if (buffer == NULL || cap < host->clipboard_len) return host->clipboard_len;
    memcpy(buffer, host->clipboard_text, (size_t)host->clipboard_len);
    return host->clipboard_len;
  }
  int fds[2];
  if (pipe(fds) != 0) return -1;
  p_wl_proxy_marshal_flags(host->data_offer, 1 /* receive */, NULL,
                           1, 0,
                           host->offered_mime, fds[1]);
  close(fds[1]);
  p_wl_display_flush(host->display);
  char *collected = NULL;
  size_t total = 0;
  for (int32_t round = 0; round < 400; round++) {
    read_socket(host, 0);
    char chunk[4096];
    ssize_t got = read(fds[0], chunk, sizeof(chunk));
    if (got > 0) {
      char *grown = (char *)realloc(collected, total + (size_t)got + 1);
      if (grown == NULL) break;
      collected = grown;
      memcpy(collected + total, chunk, (size_t)got);
      total += (size_t)got;
      continue;
    }
    if (got == 0) break;
    if (errno == EAGAIN) {
      struct timeval timeout = { 0, 5000 };
      fd_set readable;
      FD_ZERO(&readable);
      FD_SET(fds[0], &readable);
      select(fds[0] + 1, &readable, NULL, NULL, &timeout);
      continue;
    }
    break;
  }
  close(fds[0]);
  if (collected == NULL) return -1;
  int32_t len = (int32_t)total;
  if (buffer != NULL && cap >= len) memcpy(buffer, collected, (size_t)len);
  free(collected);
  return len;
}

MOONBIT_FFI_EXPORT
void wl_clipboard_set(moonapp_host *host, const char *text, int32_t len) {
  if (host == NULL || host->data_device_manager == NULL) return;
  free(host->clipboard_text);
  host->clipboard_text = (char *)malloc((size_t)len + 1);
  if (host->clipboard_text == NULL) {
    host->clipboard_len = 0;
    return;
  }
  memcpy(host->clipboard_text, text, (size_t)len);
  host->clipboard_text[len] = 0;
  host->clipboard_len = len;
  if (host->data_source != NULL) {
    p_wl_proxy_marshal_flags(host->data_source, 1 /* destroy */, NULL,
                             1,
                             WL_MARSHAL_FLAG_DESTROY);
  }
  host->data_source = p_wl_proxy_marshal_flags(
      host->data_device_manager, 0 /* create_data_source */,
      wl_data_source_interface, 3, 0, NULL);
  if (host->data_source == NULL) return;
  struct wl_data_source_listener_ *source =
      (struct wl_data_source_listener_ *)host->data_source_listener;
  p_wl_proxy_add_listener(host->data_source, (void (**)(void))source, host);
  p_wl_proxy_marshal_flags(host->data_source, 0 /* offer */, NULL,
                           1, 0,
                           "text/plain;charset=utf-8");
  p_wl_proxy_marshal_flags(host->data_source, 0 /* offer */, NULL,
                           1, 0,
                           "text/plain");
  if (host->data_device != NULL) {
    p_wl_proxy_marshal_flags(host->data_device, 0 /* set_selection */, NULL,
                             1, 0,
                             host->data_source, 0);
  }
  p_wl_display_flush(host->display);
}

/* ------------------------------------------------------------------------- */
/* Input methods, native handles, test hooks                                  */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
void wl_set_ime_allowed(moonapp_host *host, int32_t id, int32_t allowed) {
  moonapp_win *win = find_window(host, id);
  if (win != NULL) win->ime_allowed = allowed;
}

MOONBIT_FFI_EXPORT
void wl_set_ime_area(moonapp_host *host, int32_t id, int32_t x, int32_t y,
                          int32_t width, int32_t height) {
  (void)host;
  (void)id;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
}

MOONBIT_FFI_EXPORT
int32_t wl_native_kind(void) { return MOONAPP_PLATFORM_WAYLAND; }

MOONBIT_FFI_EXPORT
uint64_t wl_native_display(moonapp_host *host) {
  return host == NULL ? 0 : (uint64_t)(uintptr_t)host->display;
}

MOONBIT_FFI_EXPORT
uint64_t wl_native_window(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  return win == NULL ? 0 : (uint64_t)(uintptr_t)win->surface;
}

MOONBIT_FFI_EXPORT
int32_t wl_test_key(moonapp_host *host, int32_t id, int32_t code,
                         int32_t down) {
  /* Nothing on the client side of Wayland may put an event into another
   * client's queue; a test drives a real compositor instead. */
  (void)host;
  (void)id;
  (void)code;
  (void)down;
  return 0;
}

MOONBIT_FFI_EXPORT
int32_t wl_test_text(moonapp_host *host, int32_t id, int32_t unit) {
  (void)host;
  (void)id;
  (void)unit;
  return 0;
}

MOONBIT_FFI_EXPORT
int32_t wl_test_pointer(moonapp_host *host, int32_t id, int32_t kind,
                             int32_t x, int32_t y, int32_t button) {
  (void)host;
  (void)id;
  (void)kind;
  (void)x;
  (void)y;
  (void)button;
  return 0;
}

#endif /* __linux__ || __unix__ */
