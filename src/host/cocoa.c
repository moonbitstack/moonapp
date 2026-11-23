/*
 * Cocoa backend, through the Objective-C runtime's C interface.
 *
 * There is no Objective-C in this repository: the classes this file needs are
 * looked up by name, the view a window draws into is a class built at run time,
 * and every message goes through the runtime's own dispatch function. The
 * frameworks are opened at run time as well, so importing this package adds no
 * linker flag to a consumer's build.
 */

#if defined(__APPLE__)

#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <moonbit.h>

#include "host_internal.h"

/* ------------------------------------------------------------------------- */
/* The runtime, and the frameworks                                           */
/* ------------------------------------------------------------------------- */

typedef struct objc_object *id;
typedef struct objc_selector *SEL;
typedef id Class;
typedef signed char BOOL_;
typedef long NSInteger;

typedef struct {
  double x;
  double y;
} CGPoint;

typedef struct {
  double width;
  double height;
} CGSize;

typedef struct {
  CGPoint origin;
  CGSize size;
} CGRect;

typedef struct {
  NSInteger location;
  NSInteger length;
} NSRange;

typedef void *CGImageRef;
typedef void *CGDataProviderRef;
typedef void *CGColorSpaceRef;

static id (*p_objc_msgSend)(id, SEL, ...);
static Class (*p_objc_getClass)(const char *);
static SEL (*p_sel_registerName)(const char *);
static Class (*p_objc_allocateClassPair)(Class, const char *, size_t);
static void (*p_objc_registerClassPair)(Class);
static int (*p_class_addMethod)(Class, SEL, void *, const char *);
static Class (*p_object_getClass)(id);

static CGColorSpaceRef (*p_CGColorSpaceCreateDeviceRGB)(void);
static CGDataProviderRef (*p_CGDataProviderCreateWithData)(void *, const void *,
                                                           size_t, void *);
static CGImageRef (*p_CGImageCreate)(size_t, size_t, size_t, size_t, size_t,
                                     CGColorSpaceRef, uint32_t,
                                     CGDataProviderRef, const void *, int,
                                     int);
static void (*p_CGImageRelease)(CGImageRef);
static void (*p_CGDataProviderRelease)(CGDataProviderRef);

static void *runtime_library;
static void *appkit_library;
static void *quartz_library;
static void *coregraphics_library;

static int32_t load_cocoa(void) {
  if (runtime_library != NULL) return 1;
  static const char *runtime_names[] = { "/usr/lib/libobjc.A.dylib",
                                         "libobjc.A.dylib", NULL };
  for (int32_t i = 0; runtime_names[i] != NULL && runtime_library == NULL; i++) {
    runtime_library = dlopen(runtime_names[i], RTLD_NOW | RTLD_GLOBAL);
  }
  if (runtime_library == NULL) return 0;
  *(void **)(&p_objc_msgSend) = dlsym(runtime_library, "objc_msgSend");
  *(void **)(&p_objc_getClass) = dlsym(runtime_library, "objc_getClass");
  *(void **)(&p_sel_registerName) = dlsym(runtime_library, "sel_registerName");
  *(void **)(&p_objc_allocateClassPair) =
      dlsym(runtime_library, "objc_allocateClassPair");
  *(void **)(&p_objc_registerClassPair) =
      dlsym(runtime_library, "objc_registerClassPair");
  *(void **)(&p_class_addMethod) = dlsym(runtime_library, "class_addMethod");
  *(void **)(&p_object_getClass) = dlsym(runtime_library, "object_getClass");
  if (p_objc_msgSend == NULL || p_objc_getClass == NULL ||
      p_sel_registerName == NULL) {
    return 0;
  }
  /* Opening the frameworks is what registers their classes with the runtime. */
  appkit_library =
      dlopen("/System/Library/Frameworks/AppKit.framework/AppKit", RTLD_NOW);
  quartz_library = dlopen("/System/Library/Frameworks/QuartzCore.framework/QuartzCore",
                          RTLD_NOW);
  coregraphics_library =
      dlopen("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics",
             RTLD_NOW);
  if (appkit_library == NULL || coregraphics_library == NULL) return 0;
  *(void **)(&p_CGColorSpaceCreateDeviceRGB) =
      dlsym(coregraphics_library, "CGColorSpaceCreateDeviceRGB");
  *(void **)(&p_CGDataProviderCreateWithData) =
      dlsym(coregraphics_library, "CGDataProviderCreateWithData");
  *(void **)(&p_CGImageCreate) = dlsym(coregraphics_library, "CGImageCreate");
  *(void **)(&p_CGImageRelease) = dlsym(coregraphics_library, "CGImageRelease");
  *(void **)(&p_CGDataProviderRelease) =
      dlsym(coregraphics_library, "CGDataProviderRelease");
  return p_CGImageCreate != NULL;
}

static SEL node(const char *name) { return p_sel_registerName(name); }

#define MSG(ret, ...) ((ret(*)(id, SEL, ...))p_objc_msgSend)
#define SEND0(ret, object, name) ((ret(*)(id, SEL))p_objc_msgSend)(object, node(name))
#define SEND1(ret, object, name, a)                                           \
  ((ret(*)(id, SEL, id))p_objc_msgSend)(object, node(name), (id)(a))
#define SEND2(ret, object, name, a, b)                                        \
  ((ret(*)(id, SEL, id, id))p_objc_msgSend)(object, node(name), (id)(a), (id)(b))

/* A CGRect return needs a different calling convention on Intel: the structure
 * does not fit in registers. */
static CGRect send_rect0(id object, const char *name) {
#if defined(__x86_64__)
  CGRect result;
  ((void (*)(CGRect *, id, SEL))p_objc_msgSend)(&result, object, node(name));
  return result;
#else
  return ((CGRect(*)(id, SEL))p_objc_msgSend)(object, node(name));
#endif
}

static CGRect send_rect1(id object, const char *name, CGRect argument) {
#if defined(__x86_64__)
  CGRect result;
  ((void (*)(CGRect *, id, SEL, CGRect))p_objc_msgSend)(&result, object,
                                                        node(name), argument);
  return result;
#else
  return ((CGRect(*)(id, SEL, CGRect))p_objc_msgSend)(object, node(name),
                                                      argument);
#endif
}

/* ------------------------------------------------------------------------- */
/* State                                                                      */
/* ------------------------------------------------------------------------- */

typedef struct moonapp_win {
  int32_t id;
  int32_t alive;
  struct moonapp_host *host;
  id window;
  id view;
  id layer;
  CGImageRef image;
  uint8_t *pixels;
  int32_t bytes;
  int32_t width;
  int32_t height;
  double scale;
  int32_t cursor;
  int32_t ime_allowed;
  int32_t shift;
  int32_t control;
  int32_t alt;
  int32_t meta;
  int32_t caps;
} moonapp_win;

typedef struct moonapp_host {
  moonapp_queue queue;
  moonapp_event current;
  int32_t has_current;
  int32_t next_id;
  moonapp_win windows[MOONAPP_MAX_WINDOWS];
  int32_t count;
  char error[MOONAPP_ERROR];
  id application;
  id view_class;
  int32_t quitting;
  void *events;
} moonapp_host;

static moonapp_host *the_host;

static moonapp_event *push_event(moonapp_host *host, int32_t kind,
                                 int32_t window) {
  moonapp_event *event = moonapp_queue_push(&host->queue);
  if (event == NULL) return NULL;
  event->kind = kind;
  event->window = window;
  return event;
}

static moonapp_win *find_window(moonapp_host *host, int32_t window) {
  for (int32_t i = 0; i < host->count; i++) {
    if (host->windows[i].alive && host->windows[i].id == window) {
      return &host->windows[i];
    }
  }
  return NULL;
}

static moonapp_win *window_of_view(moonapp_host *host, id view) {
  for (int32_t i = 0; i < host->count; i++) {
    if (host->windows[i].alive && host->windows[i].view == view) {
      return &host->windows[i];
    }
  }
  return NULL;
}

static int32_t modifiers_now(moonapp_win *win) {
  int32_t bits = 0;
  if (win->shift) bits |= 1;
  if (win->control) bits |= 2;
  if (win->alt) bits |= 4;
  if (win->meta) bits |= 8;
  if (win->caps) bits |= 16;
  return bits;
}

/* An NSString as UTF-8, copied into the event's own buffer. */
static void emit_string(moonapp_host *host, int32_t kind, int32_t window,
                        id string) {
  if (string == NULL) return;
  const char *utf8 = SEND0(const char *, string, "UTF8String");
  if (utf8 == NULL) return;
  moonapp_event *event = push_event(host, kind, window);
  if (event == NULL) return;
  moonapp_event_set_text(event, utf8, (int32_t)strlen(utf8));
}

/* ------------------------------------------------------------------------- */
/* The view                                                                   */
/* ------------------------------------------------------------------------- */

static void view_draw_rect(id self, SEL _cmd, CGRect rect) {
  (void)_cmd;
  (void)rect;
  moonapp_host *host = the_host;
  moonapp_win *win = host == NULL ? NULL : window_of_view(host, self);
  if (win == NULL) return;
  /* The image is the frame: the layer holds it, and drawing is only what makes
   * the view have any content of its own. */
  if (win->layer != NULL && win->image != NULL) {
    SEND1(void, win->layer, "setContents:", win->image);
  }
}

static BOOL_ view_is_flipped(id self, SEL _cmd) {
  (void)self;
  (void)_cmd;
  return 1;
}

static BOOL_ view_accepts_first_responder(id self, SEL _cmd) {
  (void)self;
  (void)_cmd;
  return 1;
}

static void view_key(id self, SEL _cmd, id event) {
  (void)_cmd;
  moonapp_host *host = the_host;
  moonapp_win *win = host == NULL ? NULL : window_of_view(host, self);
  if (win == NULL) return;
  NSInteger type = (NSInteger)(intptr_t)SEND0(NSInteger, event, "type");
  int32_t down = type == 10 /* NSEventTypeKeyDown */;
  NSInteger code = (NSInteger)(intptr_t)SEND0(id, event, "keyCode");
  moonapp_event *pressed =
      push_event(host, down ? MOONAPP_EV_KEY_DOWN : MOONAPP_EV_KEY_UP, win->id);
  if (pressed != NULL) {
    pressed->i32a = (int32_t)code;
    pressed->i32b = modifiers_now(win);
    pressed->i32c = (int32_t)(intptr_t)SEND0(BOOL_, event, "isARepeat") != 0;
    pressed->i32d = (int32_t)code;
  }
  if (!down) return;
  /* Let the text system have the event: it turns a keystroke into characters,
   * and a composition into marked text. */
  id array = SEND1(id, p_objc_getClass("NSArray"), "arrayWithObject:", event);
  if (array != NULL) SEND1(void, self, "interpretKeyEvents:", array);
}

static void view_flags_changed(id self, SEL _cmd, id event) {
  (void)_cmd;
  moonapp_host *host = the_host;
  moonapp_win *win = host == NULL ? NULL : window_of_view(host, self);
  if (win == NULL) return;
  uint64_t flags = (uint64_t)(uintptr_t)SEND0(id, event, "modifierFlags");
  win->shift = (flags & (1u << 17)) != 0;   /* NSEventModifierFlagShift */
  win->control = (flags & (1u << 18)) != 0; /* Control */
  win->alt = (flags & (1u << 19)) != 0;     /* Option */
  win->meta = (flags & (1u << 20)) != 0;    /* Command */
  win->caps = (flags & (1u << 16)) != 0;    /* CapsLock */
  NSInteger code = (NSInteger)(intptr_t)SEND0(id, event, "keyCode");
  int32_t down = (code == 56 || code == 60) ? win->shift
                  : (code == 59 || code == 62) ? win->control
                  : (code == 58 || code == 61) ? win->alt
                  : (code == 55 || code == 54) ? win->meta
                                               : 0;
  moonapp_event *pressed = push_event(
      host, down ? MOONAPP_EV_KEY_DOWN : MOONAPP_EV_KEY_UP, win->id);
  if (pressed != NULL) {
    pressed->i32a = (int32_t)code;
    pressed->i32b = modifiers_now(win);
    pressed->i32d = (int32_t)code;
  }
}

/* NSTextInputClient, the part the text system calls into. */
static void view_insert_text(id self, SEL _cmd, id string, NSRange range) {
  (void)_cmd;
  (void)range;
  moonapp_host *host = the_host;
  moonapp_win *win = host == NULL ? NULL : window_of_view(host, self);
  if (win == NULL) return;
  emit_string(host, MOONAPP_EV_TEXT, win->id, string);
}

static void view_set_marked_text(id self, SEL _cmd, id string, NSRange selected,
                                 NSRange replacement) {
  (void)_cmd;
  (void)selected;
  (void)replacement;
  moonapp_host *host = the_host;
  moonapp_win *win = host == NULL ? NULL : window_of_view(host, self);
  if (win == NULL || !win->ime_allowed) return;
  if (string == NULL) {
    push_event(host, MOONAPP_EV_IME_DISABLED, win->id);
    return;
  }
  const char *utf8 = SEND0(const char *, string, "UTF8String");
  moonapp_event *event = push_event(host, MOONAPP_EV_IME_PREEDIT, win->id);
  if (event == NULL) return;
  if (utf8 != NULL) moonapp_event_set_text(event, utf8, (int32_t)strlen(utf8));
  event->i32a = 0;
  event->i32b = 0;
}

static void view_unmark_text(id self, SEL _cmd) {
  (void)_cmd;
  moonapp_host *host = the_host;
  moonapp_win *win = host == NULL ? NULL : window_of_view(host, self);
  if (win != NULL) push_event(host, MOONAPP_EV_IME_DISABLED, win->id);
}

static BOOL_ view_has_marked_text(id self, SEL _cmd) {
  (void)self;
  (void)_cmd;
  return 0;
}

static NSRange view_marked_range(id self, SEL _cmd) {
  (void)self;
  (void)_cmd;
  NSRange range = { -1, 0 };
  return range;
}

static NSRange view_selected_range(id self, SEL _cmd) {
  (void)self;
  (void)_cmd;
  NSRange range = { 0, 0 };
  return range;
}

static void view_do_command(id self, SEL _cmd, SEL selector) {
  (void)self;
  (void)_cmd;
  (void)selector;
}

static void view_mouse(id self, SEL _cmd, id event) {
  (void)_cmd;
  moonapp_host *host = the_host;
  moonapp_win *win = host == NULL ? NULL : window_of_view(host, self);
  if (win == NULL) return;
  CGPoint at = ((CGPoint(*)(id, SEL))p_objc_msgSend)(event, node("locationInWindow"));
  moonapp_event *moved = push_event(host, MOONAPP_EV_POINTER_MOVED, win->id);
  if (moved != NULL) {
    moved->f64a = at.x;
    /* Cocoa counts from the bottom left; a frame counts from the top left. */
    moved->f64b =
        (double)win->height / win->scale - at.y;
  }
}

static void view_mouse_button(id self, SEL _cmd, id event, int32_t button,
                              int32_t down) {
  (void)_cmd;
  moonapp_host *host = the_host;
  moonapp_win *win = host == NULL ? NULL : window_of_view(host, self);
  if (win == NULL) return;
  CGPoint at = ((CGPoint(*)(id, SEL))p_objc_msgSend)(event, node("locationInWindow"));
  moonapp_event *moved = push_event(host, MOONAPP_EV_POINTER_MOVED, win->id);
  if (moved != NULL) {
    moved->f64a = at.x;
    moved->f64b = (double)win->height / win->scale - at.y;
  }
  moonapp_event *event_out = push_event(
      host, down ? MOONAPP_EV_BUTTON_DOWN : MOONAPP_EV_BUTTON_UP, win->id);
  if (event_out != NULL) {
    event_out->i32a = button;
    event_out->i32b = modifiers_now(win);
  }
}

static void view_mouse_down(id self, SEL _cmd, id event) {
  view_mouse_button(self, _cmd, event, 0, 1);
}

static void view_mouse_up(id self, SEL _cmd, id event) {
  view_mouse_button(self, _cmd, event, 0, 0);
}

static void view_right_down(id self, SEL _cmd, id event) {
  view_mouse_button(self, _cmd, event, 1, 1);
}

static void view_right_up(id self, SEL _cmd, id event) {
  view_mouse_button(self, _cmd, event, 1, 0);
}

static void view_other_down(id self, SEL _cmd, id event) {
  view_mouse_button(self, _cmd, event, 2, 1);
}

static void view_other_up(id self, SEL _cmd, id event) {
  view_mouse_button(self, _cmd, event, 2, 0);
}

static void view_scroll(id self, SEL _cmd, id event) {
  (void)_cmd;
  moonapp_host *host = the_host;
  moonapp_win *win = host == NULL ? NULL : window_of_view(host, self);
  if (win == NULL) return;
  double dy = ((double(*)(id, SEL))p_objc_msgSend)(event, node("scrollingDeltaY"));
  moonapp_event *event_out = push_event(host, MOONAPP_EV_WHEEL, win->id);
  if (event_out != NULL) {
    event_out->i32a = 1; /* a trackpad reports pixels, not notches */
    event_out->f64b = dy;
  }
}

static void view_enter(id self, SEL _cmd, id event) {
  (void)_cmd;
  (void)event;
  moonapp_host *host = the_host;
  moonapp_win *win = host == NULL ? NULL : window_of_view(host, self);
  if (win != NULL) push_event(host, MOONAPP_EV_POINTER_ENTERED, win->id);
}

static void view_exit(id self, SEL _cmd, id event) {
  (void)_cmd;
  (void)event;
  moonapp_host *host = the_host;
  moonapp_win *win = host == NULL ? NULL : window_of_view(host, self);
  if (win != NULL) push_event(host, MOONAPP_EV_POINTER_LEFT, win->id);
}

static void view_resized(id self, SEL _cmd) {
  (void)_cmd;
  moonapp_host *host = the_host;
  moonapp_win *win = host == NULL ? NULL : window_of_view(host, self);
  if (win == NULL) return;
  CGRect bounds = send_rect0(self, "bounds");
  int32_t width = (int32_t)(bounds.size.width * win->scale);
  int32_t height = (int32_t)(bounds.size.height * win->scale);
  if (width == win->width && height == win->height) return;
  win->width = width;
  win->height = height;
  moonapp_event *event = push_event(host, MOONAPP_EV_RESIZED, win->id);
  if (event != NULL) {
    event->i32a = width;
    event->i32b = height;
    event->f64a = win->scale;
  }
}

/* The class the runtime builds: a plain view with the messages above. */
static Class make_view_class(void) {
  Class base = p_objc_getClass("NSView");
  Class view = p_objc_allocateClassPair(base, "MoonappView", 0);
  if (view == NULL) return NULL;
  p_class_addMethod(view, node("drawRect:"), (void *)view_draw_rect, "v@:{CGRect={CGPoint=dd}{CGSize=dd}}");
  p_class_addMethod(view, node("isFlipped"), (void *)view_is_flipped, "c@:");
  p_class_addMethod(view, node("acceptsFirstResponder"),
                    (void *)view_accepts_first_responder, "c@:");
  p_class_addMethod(view, node("keyDown:"), (void *)view_key, "v@:@");
  p_class_addMethod(view, node("keyUp:"), (void *)view_key, "v@:@");
  p_class_addMethod(view, node("flagsChanged:"), (void *)view_flags_changed, "v@:@");
  p_class_addMethod(view, node("insertText:replacementRange:"),
                    (void *)view_insert_text, "v@:@{_NSRange=qq}");
  p_class_addMethod(view, node("setMarkedText:selectedRange:replacementRange:"),
                    (void *)view_set_marked_text, "v@:@{_NSRange=qq}{_NSRange=qq}");
  p_class_addMethod(view, node("unmarkText"), (void *)view_unmark_text, "v@:");
  p_class_addMethod(view, node("hasMarkedText"), (void *)view_has_marked_text, "c@:");
  p_class_addMethod(view, node("markedRange"), (void *)view_marked_range,
                    "{_NSRange=qq}@:");
  p_class_addMethod(view, node("selectedRange"), (void *)view_selected_range,
                    "{_NSRange=qq}@:");
  p_class_addMethod(view, node("doCommandBySelector:"), (void *)view_do_command,
                    "v@::");
  p_class_addMethod(view, node("mouseMoved:"), (void *)view_mouse, "v@:@");
  p_class_addMethod(view, node("mouseDragged:"), (void *)view_mouse, "v@:@");
  p_class_addMethod(view, node("mouseDown:"), (void *)view_mouse_down, "v@:@");
  p_class_addMethod(view, node("mouseUp:"), (void *)view_mouse_up, "v@:@");
  p_class_addMethod(view, node("rightMouseDown:"), (void *)view_right_down, "v@:@");
  p_class_addMethod(view, node("rightMouseUp:"), (void *)view_right_up, "v@:@");
  p_class_addMethod(view, node("otherMouseDown:"), (void *)view_other_down, "v@:@");
  p_class_addMethod(view, node("otherMouseUp:"), (void *)view_other_up, "v@:@");
  p_class_addMethod(view, node("scrollWheel:"), (void *)view_scroll, "v@:@");
  p_class_addMethod(view, node("mouseEntered:"), (void *)view_enter, "v@:@");
  p_class_addMethod(view, node("mouseExited:"), (void *)view_exit, "v@:@");
  p_class_addMethod(view, node("setFrameSize:"), (void *)view_resized,
                    "v@:{CGSize=dd}");
  p_objc_registerClassPair(view);
  return view;
}

/* ------------------------------------------------------------------------- */
/* Host                                                                       */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
void *moonapp_host_new(void) {
  if (!load_cocoa()) return NULL;
  moonapp_host *host = (moonapp_host *)calloc(1, sizeof(moonapp_host));
  if (host == NULL) return NULL;
  moonapp_queue_init(&host->queue);
  host->next_id = 1;
  host->view_class = make_view_class();
  if (host->view_class == NULL) {
    free(host);
    return NULL;
  }
  host->application = SEND0(id, p_objc_getClass("NSApplication"),
                            "sharedApplication");
  if (host->application == NULL) {
    free(host);
    return NULL;
  }
  /* A regular application: it gets a menu bar, a Dock icon and a window that can
   * take the keyboard. */
  SEND1(void, host->application, "setActivationPolicy:", (void *)0);
  SEND0(void, host->application, "finishLaunching");
  SEND0(void, host->application, "activateIgnoringOtherApps:");
  the_host = host;
  return host;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_host_ok(void *host) { return host != NULL ? 1 : 0; }

MOONBIT_FFI_EXPORT
int32_t moonapp_host_error(void *host, char *into, int32_t cap) {
  const char *message = host == NULL ? "no window server to connect to"
                                     : ((moonapp_host *)host)->error;
  if (into == NULL || cap <= 0) return 0;
  int32_t len = (int32_t)strlen(message);
  if (len > cap - 1) len = cap - 1;
  memcpy(into, message, (size_t)len);
  into[len] = 0;
  return len;
}

MOONBIT_FFI_EXPORT
void moonapp_host_free(void *raw) {
  if (raw == NULL) return;
  moonapp_host *host = (moonapp_host *)raw;
  for (int32_t i = 0; i < host->count; i++) {
    if (!host->windows[i].alive) continue;
    moonapp_win *win = &host->windows[i];
    if (win->window != NULL) SEND0(void, win->window, "close");
    if (win->image != NULL) p_CGImageRelease(win->image);
    free(win->pixels);
    win->alive = 0;
  }
  host->count = 0;
  the_host = NULL;
  free(host);
}

MOONBIT_FFI_EXPORT
double moonapp_primary_scale(void *raw) {
  if (raw == NULL) return 1.0;
  id screen = SEND0(id, p_objc_getClass("NSScreen"), "mainScreen");
  if (screen == NULL) return 1.0;
  double scale = ((double(*)(id, SEL))p_objc_msgSend)(screen,
                                                     node("backingScaleFactor"));
  return scale > 0.0 ? scale : 1.0;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_is_shutdown(void *raw) {
  moonapp_host *host = (moonapp_host *)raw;
  return host == NULL ? 1 : host->quitting;
}

MOONBIT_FFI_EXPORT
void moonapp_host_shutdown(void *raw) {
  moonapp_host *host = (moonapp_host *)raw;
  if (host != NULL) host->quitting = 1;
}

static id make_string(const char *text, int32_t len) {
  id alloc = SEND0(id, p_objc_getClass("NSString"), "alloc");
  if (alloc == NULL) return NULL;
  return ((id(*)(id, SEL, const char *, NSInteger, NSInteger))p_objc_msgSend)(
      alloc, node("initWithBytes:length:encoding:"), text, (NSInteger)len, 4);
}

MOONBIT_FFI_EXPORT
int32_t moonapp_open(void *raw, const char *title, int32_t title_len,
                     const char *app_id, int32_t app_id_len, int32_t width,
                     int32_t height, int32_t flags) {
  (void)app_id;
  (void)app_id_len;
  moonapp_host *host = (moonapp_host *)raw;
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
  win->scale = moonapp_primary_scale(raw);
  win->width = width > 0 ? width : 1;
  win->height = height > 0 ? height : 1;
  win->cursor = 0;
  win->ime_allowed = 1;
  /* The window's content is the window's size in points, which is the physical
   * size the caller asked for, divided by the scale. */
  CGRect content = { { 0, 0 },
                     { (double)win->width / win->scale,
                       (double)win->height / win->scale } };
  uint64_t style = 0;
  if (flags & MOONAPP_FLAG_DECORATIONS) {
    style |= 1 | 2 | 4; /* titled, closable, miniaturizable */
  }
  if (flags & MOONAPP_FLAG_RESIZABLE) style |= 8;
  id window = SEND0(id, p_objc_getClass("NSWindow"), "alloc");
  if (window == NULL) {
    win->alive = 0;
    return -1;
  }
  window = ((id(*)(id, SEL, CGRect, uint64_t, uint64_t, BOOL_))p_objc_msgSend)(
      window, node("initWithContentRect:styleMask:backing:defer:"), content,
      style, 2 /* NSBackingStoreBuffered */, 0);
  if (window == NULL) {
    moonapp_error(host->error, "the window server refused the window");
    win->alive = 0;
    return -1;
  }
  win->window = window;
  id view = SEND0(id, SEND0(id, host->view_class, "alloc"), "init");
  if (view == NULL) {
    SEND0(void, window, "release");
    win->alive = 0;
    return -1;
  }
  win->view = view;
  SEND1(void, view, "setWantsLayer:", (void *)1);
  SEND1(void, window, "setContentView:", view);
  SEND1(void, window, "makeFirstResponder:", view);
  win->layer = SEND0(id, view, "layer");
  SEND1(void, window, "setReleasedWhenClosed:", (void *)0);
  if (flags & MOONAPP_FLAG_TRANSPARENT) {
    SEND1(void, window, "setOpaque:", (void *)0);
    SEND1(void, window, "setBackgroundColor:", SEND0(id, p_objc_getClass("NSColor"), "clearColor"));
  }
  if (title_len > 0) {
    id text = make_string(title, title_len);
    if (text != NULL) SEND1(void, window, "setTitle:", text);
  }
  if (flags & MOONAPP_FLAG_VISIBLE) {
    SEND1(void, window, "makeKeyAndOrderFront:", NULL);
    SEND1(void, host->application, "activateIgnoringOtherApps:", (void *)1);
  }
  /* Ask for mouse moves without a button held down. */
  SEND1(void, window, "setAcceptsMouseMovedEvents:", (void *)1);
  host->count++;
  return win->id;
}

MOONBIT_FFI_EXPORT
void moonapp_close(void *raw, int32_t window) {
  moonapp_host *host = (moonapp_host *)raw;
  if (host == NULL) return;
  moonapp_win *win = find_window(host, window);
  if (win == NULL) return;
  if (win->window != NULL) SEND0(void, win->window, "close");
  if (win->image != NULL) {
    p_CGImageRelease(win->image);
    win->image = NULL;
  }
  free(win->pixels);
  win->pixels = NULL;
  win->alive = 0;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_window_open(void *raw, int32_t window) {
  moonapp_host *host = (moonapp_host *)raw;
  if (host == NULL) return 0;
  return find_window(host, window) != NULL;
}

/* Cocoa delivers an event by sending it to the application, which hands it to
 * the window it belongs to. */
static void pump_once(moonapp_host *host, int32_t timeout_ms) {
  id distant = SEND0(id, p_objc_getClass("NSDate"), "distantPast");
  if (timeout_ms > 0) {
    distant = MSG(id, id, SEL, double)(
        p_objc_getClass("NSDate"), node("dateWithTimeIntervalSinceNow:"),
        (double)timeout_ms / 1000.0);
  }
  uint64_t mask = 0xFFFFFFFFFFFFFFFFull;
  id event = ((id(*)(id, SEL, uint64_t, id, id, BOOL_))p_objc_msgSend)(
      host->application, node("nextEventMatchingMask:untilDate:inMode:dequeue:"),
      mask, distant, SEND0(id, p_objc_getClass("NSRunLoop"), "currentMode"), 1);
  if (event == NULL) return;
  SEND1(void, host->application, "sendEvent:", event);
}

MOONBIT_FFI_EXPORT
int32_t moonapp_poll(void *raw) {
  moonapp_host *host = (moonapp_host *)raw;
  if (host == NULL) return 0;
  if (host->has_current) return 1;
  if (moonapp_queue_pop(&host->queue, &host->current)) {
    host->has_current = 1;
    return 1;
  }
  pump_once(host, 0);
  if (moonapp_queue_pop(&host->queue, &host->current)) {
    host->has_current = 1;
    return 1;
  }
  return 0;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_wait(void *raw, int32_t timeout_ms) {
  moonapp_host *host = (moonapp_host *)raw;
  if (host == NULL) return 0;
  if (host->has_current) return 1;
  if (moonapp_queue_pop(&host->queue, &host->current)) {
    host->has_current = 1;
    return 1;
  }
  pump_once(host, timeout_ms > 0 ? timeout_ms : 16);
  if (moonapp_queue_pop(&host->queue, &host->current)) {
    host->has_current = 1;
    return 1;
  }
  return 0;
}

MOONBIT_FFI_EXPORT
void moonapp_event_release(void *raw) {
  moonapp_host *host = (moonapp_host *)raw;
  if (host != NULL) host->has_current = 0;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_event_kind(void *raw) {
  return ((moonapp_host *)raw)->current.kind;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_event_window(void *raw) {
  return ((moonapp_host *)raw)->current.window;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_event_i32(void *raw, int32_t which) {
  moonapp_host *host = (moonapp_host *)raw;
  switch (which) {
    case 0: return host->current.i32a;
    case 1: return host->current.i32b;
    case 2: return host->current.i32c;
    default: return host->current.i32d;
  }
}

MOONBIT_FFI_EXPORT
double moonapp_event_f64(void *raw, int32_t which) {
  moonapp_host *host = (moonapp_host *)raw;
  return which == 0 ? host->current.f64a : host->current.f64b;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_event_text_len(void *raw) {
  return ((moonapp_host *)raw)->current.text_len;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_event_text(void *raw, uint8_t *buffer, int32_t cap) {
  moonapp_host *host = (moonapp_host *)raw;
  int32_t len = host->current.text_len;
  if (buffer == NULL || cap < len) return len;
  memcpy(buffer, host->current.text, (size_t)len);
  return len;
}

MOONBIT_FFI_EXPORT
void moonapp_set_title(void *raw, int32_t window, const char *title, int32_t len) {
  moonapp_host *host = (moonapp_host *)raw;
  moonapp_win *win = host == NULL ? NULL : find_window(host, window);
  if (win == NULL) return;
  id text = make_string(title, len);
  if (text != NULL) SEND1(void, win->window, "setTitle:", text);
}

MOONBIT_FFI_EXPORT
void moonapp_set_size(void *raw, int32_t window, int32_t width, int32_t height) {
  moonapp_host *host = (moonapp_host *)raw;
  moonapp_win *win = host == NULL ? NULL : find_window(host, window);
  if (win == NULL) return;
  CGRect frame = send_rect0(win->window, "frame");
  CGRect content = { { 0, 0 },
                     { (double)width / win->scale, (double)height / win->scale } };
  CGRect outer = send_rect1(win->window,
                            "frameRectForContentRect:", content);
  frame.size = outer.size;
  ((void(*)(id, SEL, CGRect, BOOL_))p_objc_msgSend)(win->window,
                                                    node("setFrame:display:"),
                                                    frame, 1);
}

MOONBIT_FFI_EXPORT
void moonapp_set_min_size(void *raw, int32_t window, int32_t width, int32_t height) {
  moonapp_host *host = (moonapp_host *)raw;
  moonapp_win *win = host == NULL ? NULL : find_window(host, window);
  if (win == NULL) return;
  CGSize size = { (double)width / win->scale, (double)height / win->scale };
  ((void(*)(id, SEL, CGSize))p_objc_msgSend)(win->window,
                                             node("setContentMinSize:"), size);
}

MOONBIT_FFI_EXPORT
int32_t moonapp_width(void *raw, int32_t window) {
  moonapp_host *host = (moonapp_host *)raw;
  moonapp_win *win = host == NULL ? NULL : find_window(host, window);
  return win == NULL ? 0 : win->width;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_height(void *raw, int32_t window) {
  moonapp_host *host = (moonapp_host *)raw;
  moonapp_win *win = host == NULL ? NULL : find_window(host, window);
  return win == NULL ? 0 : win->height;
}

MOONBIT_FFI_EXPORT
double moonapp_scale(void *raw, int32_t window) {
  moonapp_host *host = (moonapp_host *)raw;
  moonapp_win *win = host == NULL ? NULL : find_window(host, window);
  if (win == NULL) return 1.0;
  double scale =
      ((double(*)(id, SEL))p_objc_msgSend)(win->window, node("backingScaleFactor"));
  if (scale > 0.0) win->scale = scale;
  return win->scale;
}

MOONBIT_FFI_EXPORT
void moonapp_set_cursor(void *raw, int32_t window, int32_t cursor) {
  moonapp_host *host = (moonapp_host *)raw;
  moonapp_win *win = host == NULL ? NULL : find_window(host, window);
  if (win == NULL) return;
  win->cursor = cursor;
  const char *name = "arrowCursor";
  switch (cursor) {
    case 1: name = "pointingHandCursor"; break;
    case 2: name = "IBeamCursor"; break;
    case 3: name = "crosshairCursor"; break;
    case 5: name = "operationNotAllowedCursor"; break;
    case 6:
    case 7: name = "arrowCursor"; break;
    case 9: name = "resizeUpDownCursor"; break;
    case 10: name = "resizeLeftRightCursor"; break;
    case 13:
    case 14: name = "openHandCursor"; break;
    case 19: name = "resizeUpDownCursor"; break;
    case 18: name = "resizeLeftRightCursor"; break;
    default: name = "arrowCursor"; break;
  }
  id shape = SEND0(id, p_objc_getClass("NSCursor"), name);
  if (shape != NULL) SEND0(void, shape, "set");
}

/* ------------------------------------------------------------------------- */
/* Frames                                                                     */
/* ------------------------------------------------------------------------- */

static void release_image(moonapp_win *win) {
  if (win->image != NULL) {
    p_CGImageRelease(win->image);
    win->image = NULL;
  }
}

MOONBIT_FFI_EXPORT
int32_t moonapp_present(void *raw, int32_t window, uint8_t *bgra, int32_t width,
                        int32_t height, int32_t stride) {
  moonapp_host *host = (moonapp_host *)raw;
  moonapp_win *win = host == NULL ? NULL : find_window(host, window);
  if (win == NULL || bgra == NULL || width <= 0 || height <= 0) return 0;
  if (!load_cocoa()) return 0;
  size_t bytes = (size_t)width * (size_t)height * 4;
  /* CoreGraphics copies what the provider hands it when told to, so the buffer
   * the frame came in is enough. */
  CGDataProviderRef provider = p_CGDataProviderCreateWithData(NULL, bgra, bytes, NULL);
  if (provider == NULL) return 0;
  CGImageRef image = p_CGImageCreate(
      (size_t)width, (size_t)height, 8, 32, (size_t)stride,
      p_CGColorSpaceCreateDeviceRGB(), 2 /* kCGImageAlphaPremultipliedFirst */,
      provider, NULL, 0, 0);
  p_CGDataProviderRelease(provider);
  if (image == NULL) return 0;
  release_image(win);
  win->image = image;
  if (win->layer != NULL) SEND1(void, win->layer, "setContents:", image);
  if (win->view != NULL) SEND0(void, win->view, "setNeedsDisplay:");
  return 1;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_read_back(void *raw, int32_t window, uint8_t *out, int32_t cap) {
  moonapp_host *host = (moonapp_host *)raw;
  moonapp_win *win = host == NULL ? NULL : find_window(host, window);
  if (win == NULL || win->view == NULL) return -1;
  int32_t width = win->width;
  int32_t height = win->height;
  if (width <= 0 || height <= 0) return -1;
  int32_t need = width * height * 4;
  if (out == NULL || cap < need) return need;
  CGRect bounds = send_rect0(win->view, "bounds");
  id rep = ((id(*)(id, SEL, CGRect))p_objc_msgSend)(
      win->view, node("bitmapImageRepForCachingDisplayInRect:"), bounds);
  fprintf(stderr, "[dbg] rep=%p bounds=%.0fx%.0f\n", (void *)rep,
          bounds.size.width, bounds.size.height);
  if (rep == NULL) return -1;
  ((void(*)(id, SEL, CGRect, id))p_objc_msgSend)(
      win->view, node("cacheDisplayInRect:toBitmapImageRep:"), bounds, rep);
  uint8_t *data = (uint8_t *)SEND0(void *, rep, "bitmapData");
  NSInteger row = (NSInteger)(intptr_t)SEND0(NSInteger, rep, "bytesPerRow");
  fprintf(stderr, "[dbg] data=%p row=%ld\n", (void *)data, (long)row);
  if (data == NULL || row <= 0) return -1;
  /* The bitmap is premultiplied BGRA with the first row at the top, once the
   * view is flipped, which this one is. */
  int32_t copy = width * 4;
  for (int32_t y = 0; y < height; y++) {
    memcpy(out + (size_t)y * (size_t)copy, data + (size_t)y * (size_t)row,
           (size_t)copy);
  }
  return need;
}

/* ------------------------------------------------------------------------- */
/* Clipboard                                                                  */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
int32_t moonapp_clipboard_get(void *raw, uint8_t *buffer, int32_t cap) {
  (void)raw;
  id pasteboard = SEND0(id, p_objc_getClass("NSPasteboard"), "generalPasteboard");
  if (pasteboard == NULL) return -1;
  id type = make_string("public.utf8-plain-text", 22);
  id text = SEND1(id, pasteboard, "stringForType:", type);
  if (text == NULL) return -1;
  const char *utf8 = SEND0(const char *, text, "UTF8String");
  if (utf8 == NULL) return -1;
  int32_t len = (int32_t)strlen(utf8);
  if (buffer != NULL && cap >= len) memcpy(buffer, utf8, (size_t)len);
  return len;
}

MOONBIT_FFI_EXPORT
void moonapp_clipboard_set(void *raw, const char *text, int32_t len) {
  (void)raw;
  id pasteboard = SEND0(id, p_objc_getClass("NSPasteboard"), "generalPasteboard");
  if (pasteboard == NULL) return;
  SEND0(NSInteger, pasteboard, "clearContents");
  id string = make_string(text, len);
  id type = make_string("public.utf8-plain-text", 22);
  if (string == NULL || type == NULL) return;
  ((BOOL_(*)(id, SEL, id, id))p_objc_msgSend)(pasteboard,
                                             node("setString:forType:"), string,
                                             type);
}

/* ------------------------------------------------------------------------- */
/* Input methods, native handles, test hooks                                  */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
void moonapp_set_ime_allowed(void *raw, int32_t window, int32_t allowed) {
  moonapp_host *host = (moonapp_host *)raw;
  moonapp_win *win = host == NULL ? NULL : find_window(host, window);
  if (win != NULL) win->ime_allowed = allowed;
}

MOONBIT_FFI_EXPORT
void moonapp_set_ime_area(void *raw, int32_t window, int32_t x, int32_t y,
                          int32_t width, int32_t height) {
  /* The candidate window follows the caret the text system is told about, which
   * is the marked range this view reports; a window has one caret and no more. */
  (void)raw;
  (void)window;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_native_kind(void) { return MOONAPP_PLATFORM_MACOS; }

MOONBIT_FFI_EXPORT
uint64_t moonapp_native_display(void *raw) {
  (void)raw;
  return (uint64_t)(uintptr_t)p_objc_getClass("NSApplication");
}

MOONBIT_FFI_EXPORT
uint64_t moonapp_native_window(void *raw, int32_t window) {
  moonapp_host *host = (moonapp_host *)raw;
  moonapp_win *win = host == NULL ? NULL : find_window(host, window);
  return win == NULL ? 0 : (uint64_t)(uintptr_t)win->view;
}

/* A test cannot hand a synthetic key event to the window server without a
 * session in front of it, so the hooks below put the events themselves in the
 * queue. Everything above them — the window, the frame, the text system — is the
 * real path a keystroke takes. */
MOONBIT_FFI_EXPORT
int32_t moonapp_test_key(void *raw, int32_t window, int32_t code, int32_t down) {
  moonapp_host *host = (moonapp_host *)raw;
  moonapp_win *win = host == NULL ? NULL : find_window(host, window);
  if (win == NULL) return 0;
  moonapp_event *event =
      push_event(host, down ? MOONAPP_EV_KEY_DOWN : MOONAPP_EV_KEY_UP, win->id);
  if (event != NULL) {
    event->i32a = code;
    event->i32b = modifiers_now(win);
    event->i32d = code;
  }
  return 1;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_test_text(void *raw, int32_t window, int32_t unit) {
  moonapp_host *host = (moonapp_host *)raw;
  moonapp_win *win = host == NULL ? NULL : find_window(host, window);
  if (win == NULL) return 0;
  moonapp_event *event = push_event(host, MOONAPP_EV_TEXT, win->id);
  if (event == NULL) return 0;
  char utf8[4];
  int32_t len = 0;
  if (unit < 0x80) {
    utf8[len++] = (char)unit;
  } else {
    utf8[len++] = (char)(0xC0 | (unit >> 6));
    utf8[len++] = (char)(0x80 | (unit & 0x3F));
  }
  moonapp_event_set_text(event, utf8, len);
  return 1;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_test_pointer(void *raw, int32_t window, int32_t kind, int32_t x,
                             int32_t y, int32_t button) {
  (void)button;
  moonapp_host *host = (moonapp_host *)raw;
  moonapp_win *win = host == NULL ? NULL : find_window(host, window);
  if (win == NULL) return 0;
  if (kind == 0) {
    moonapp_event *moved = push_event(host, MOONAPP_EV_POINTER_MOVED, win->id);
    if (moved != NULL) {
      moved->f64a = x;
      moved->f64b = y;
    }
    return 1;
  }
  if (kind == 5) {
    moonapp_event *wheel = push_event(host, MOONAPP_EV_WHEEL, win->id);
    if (wheel != NULL) {
      wheel->i32a = 1;
      wheel->f64b = y;
    }
    return 1;
  }
  moonapp_event *event = push_event(
      host, kind == 1 || kind == 3 ? MOONAPP_EV_BUTTON_DOWN : MOONAPP_EV_BUTTON_UP,
      win->id);
  if (event != NULL) {
    event->i32a = kind <= 2 ? 0 : 1;
  }
  return 1;
}

#endif /* __APPLE__ */
