/*
 * X11 backend: a window on the display server, an XImage shared with it, and
 * the server's event queue as the source of input.
 *
 * The library is opened at run time rather than linked, so importing this
 * package adds no linker flag to a consumer's build. Everything below talks to
 * the same set of `moonapp_*` entry points the other backends implement.
 */

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <moonbit.h>

#include "host_internal.h"

/* ------------------------------------------------------------------------- */
/* The library, resolved once                                                 */
/* ------------------------------------------------------------------------- */

#define MOONAPP_X11_FUNCTIONS(V)                                              \
  V(Display *, XOpenDisplay, (const char *))                                  \
  V(int, XCloseDisplay, (Display *))                                          \
  V(int, XPending, (Display *))                                               \
  V(int, XNextEvent, (Display *, XEvent *))                                   \
  V(int, XFlush, (Display *))                                                 \
  V(int, XSync, (Display *, Bool))                                            \
  V(Window, XCreateWindow,                                                    \
    (Display *, Window, int, int, unsigned int, unsigned int, unsigned int,    \
     int, unsigned int, Visual *, unsigned long, XSetWindowAttributes *))      \
  V(int, XDestroyWindow, (Display *, Window))                                 \
  V(int, XMapWindow, (Display *, Window))                                     \
  V(int, XUnmapWindow, (Display *, Window))                                   \
  V(int, XResizeWindow, (Display *, Window, unsigned int, unsigned int))      \
  V(int, XMoveResizeWindow,                                                   \
    (Display *, Window, int, int, unsigned int, unsigned int))                \
  V(int, XStoreName, (Display *, Window, const char *))                       \
  V(GC, XCreateGC, (Display *, Drawable, unsigned long, XGCValues *))         \
  V(int, XFreeGC, (Display *, GC))                                            \
  V(int, XPutImage,                                                           \
    (Display *, Drawable, GC, XImage *, int, int, int, int, unsigned int,      \
     unsigned int))                                                           \
  V(XImage *, XCreateImage,                                                   \
    (Display *, Visual *, unsigned int, int, int, char *, unsigned int,        \
     unsigned int, int, int))                                                 \
  V(XImage *, XGetImage,                                                      \
    (Display *, Drawable, int, int, unsigned int, unsigned int,                \
     unsigned long, int))                                                     \
  V(int, XDestroyImage, (XImage *))                                           \
  V(int, XGetWindowAttributes, (Display *, Window, XWindowAttributes *))       \
  V(Atom, XInternAtom, (Display *, const char *, Bool))                       \
  V(int, XSetWMProtocols, (Display *, Window, Atom *, int))                   \
  V(int, XSetWMNormalHints, (Display *, Window, XSizeHints *))                \
  V(int, XSetClassHint, (Display *, Window, XClassHint *))                    \
  V(int, XChangeProperty,                                                     \
    (Display *, Window, Atom, Atom, int, int, const unsigned char *, int))    \
  V(int, XDeleteProperty, (Display *, Window, Atom))                          \
  V(int, XGetWindowProperty,                                                  \
    (Display *, Window, Atom, long, long, Bool, Atom, Atom *, int *,           \
     unsigned long *, unsigned long *, unsigned char **))                     \
  V(Atom, XGetSelectionOwner, (Display *, Atom))                              \
  V(Atom, XSetSelectionOwner, (Display *, Atom, Window, Time))                \
  V(int, XConvertSelection,                                                   \
    (Display *, Atom, Atom, Atom, Window, Time))                              \
  V(int, XSendEvent, (Display *, Window, Bool, long, XEvent *))               \
  V(Cursor, XCreateFontCursor, (Display *, unsigned int))                     \
  V(Cursor, XCreatePixmapCursor,                                              \
    (Display *, Pixmap, Pixmap, XColor *, XColor *, unsigned int,              \
     unsigned int))                                                          \
  V(int, XDefineCursor, (Display *, Window, Cursor))                          \
  V(int, XFreeCursor, (Display *, Cursor))                                    \
  V(int, XFreePixmap, (Display *, Pixmap))                                    \
  V(Pixmap, XCreatePixmap, (Display *, Drawable, unsigned int, unsigned int,   \
                            unsigned int))                                   \
  V(int, XFree, (void *))                                                     \
  V(char *, XResourceManagerString, (Display *))                              \
  V(KeySym, XLookupKeysym, (XKeyEvent *, int))                                \
  V(int, XLookupString,                                                       \
    (XKeyEvent *, char *, int, KeySym *, XComposeStatus *))                   \
  V(KeyCode, XKeysymToKeycode, (Display *, KeySym))                           \
  V(XIM, XOpenIM, (Display *, XrmDatabase, char *, char *))                  \
  V(int, XCloseIM, (XIM))                                                     \
  V(int, XFillRectangle,                                                      \
    (Display *, Drawable, GC, int, int, unsigned int, unsigned int))          \
  V(int, XSetForeground, (Display *, GC, unsigned long))                   \
  V(XIC, XCreateIC, (XIM, ...))                                               \
  V(void, XDestroyIC, (XIC))                                                  \
  V(int, XSetICFocus, (XIC))                                                  \
  V(int, XUnsetICFocus, (XIC))                                                \
  V(int, XFilterEvent, (XEvent *, Window))                                    \
  V(int, Xutf8LookupString,                                                   \
    (XIC, XKeyPressedEvent *, char *, int, KeySym *, Status *))

#define MOONAPP_X11_DECLARE(type, name, args) static type (*p_##name) args;
MOONAPP_X11_FUNCTIONS(MOONAPP_X11_DECLARE)
#undef MOONAPP_X11_DECLARE

static void *x11_library;

static int32_t load_x11(void) {
  if (x11_library != NULL) return 1;
  static const char *names[] = { "libX11.so.6", "libX11.so", NULL };
  for (int32_t i = 0; names[i] != NULL && x11_library == NULL; i++) {
    x11_library = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
  }
  if (x11_library == NULL) return 0;
#define MOONAPP_X11_RESOLVE(type, name, args)                                 \
  *(void **)(&p_##name) = dlsym(x11_library, #name);                          \
  if (p_##name == NULL) return 0;
  MOONAPP_X11_FUNCTIONS(MOONAPP_X11_RESOLVE)
#undef MOONAPP_X11_RESOLVE
  return 1;
}

/* ------------------------------------------------------------------------- */
/* State                                                                      */
/* ------------------------------------------------------------------------- */

typedef struct moonapp_win {
  int32_t id;
  int32_t alive;
  Window window;
  GC gc;
  XImage *image;
  char *image_data;
  int32_t width;
  int32_t height;
  double scale;
  int32_t cursor;
  Cursor cursor_handle;
  int32_t ime_allowed;
  XIC ic;
  struct moonapp_host *host;
} moonapp_win;

typedef struct moonapp_host {
  moonapp_queue queue;
  moonapp_event current;
  int32_t has_current;
  int32_t next_id;
  moonapp_win windows[MOONAPP_MAX_WINDOWS];
  int32_t count;
  char error[MOONAPP_ERROR];
  Display *display;
  int32_t screen;
  Visual *visual;
  int32_t depth;
  Window root;
  Atom wm_delete;
  Atom net_wm_name;
  Atom utf8_string;
  Atom clipboard;
  Atom property;
  Atom targets;
  Atom incr;
  XIM im;
  char *clipboard_text;
  int32_t clipboard_len;
  int32_t quitting;
} moonapp_host;

/* ------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* ------------------------------------------------------------------------- */

static moonapp_event *push_event(moonapp_host *host, int32_t kind,
                                 int32_t window) {
  moonapp_event *event = moonapp_queue_push(&host->queue);
  if (event == NULL) return NULL;
  event->kind = kind;
  event->window = window;
  return event;
}

static int32_t modifiers_of(unsigned int state) {
  int32_t bits = 0;
  if (state & ShiftMask) bits |= 1;
  if (state & ControlMask) bits |= 2;
  if (state & Mod1Mask) bits |= 4;
  if (state & Mod4Mask) bits |= 8;
  if (state & LockMask) bits |= 16;
  if (state & Mod2Mask) bits |= 32;
  return bits;
}

static moonapp_win *find_window(moonapp_host *host, int32_t id) {
  for (int32_t i = 0; i < host->count; i++) {
    if (host->windows[i].alive && host->windows[i].id == id) {
      return &host->windows[i];
    }
  }
  return NULL;
}

static moonapp_win *window_of(moonapp_host *host, Window window) {
  for (int32_t i = 0; i < host->count; i++) {
    if (host->windows[i].alive && host->windows[i].window == window) {
      return &host->windows[i];
    }
  }
  return NULL;
}

static double read_scale(moonapp_host *host) {
  const char *resources = p_XResourceManagerString(host->display);
  if (resources != NULL) {
    const char *at = strstr(resources, "Xft.dpi:");
    if (at != NULL) {
      at += 8;
      while (*at == ' ' || *at == '\t') at++;
      double dpi = atof(at);
      if (dpi > 0.0) return dpi / 96.0;
    }
  }
  return 1.0;
}

/* ------------------------------------------------------------------------- */
/* Cursors                                                                    */
/* ------------------------------------------------------------------------- */

static void apply_cursor(moonapp_win *win, int32_t shape) {
  moonapp_host *host = win->host;
  if (win->cursor_handle != None) {
    p_XFreeCursor(host->display, win->cursor_handle);
    win->cursor_handle = None;
  }
  if (shape == 20) {
    /* A hidden pointer is a one-bit cursor that is nowhere set: the server has
     * no shape of its own that means "nothing". */
    Pixmap bitmap = p_XCreatePixmap(host->display, win->window, 1, 1, 1);
    if (bitmap != None) {
      GC gc = p_XCreateGC(host->display, bitmap, 0, NULL);
      if (gc != NULL) {
        p_XSetForeground(host->display, gc, 0);
        p_XFillRectangle(host->display, bitmap, gc, 0, 0, 1, 1);
        p_XFreeGC(host->display, gc);
      }
      XColor black;
      memset(&black, 0, sizeof(black));
      win->cursor_handle = p_XCreatePixmapCursor(host->display, bitmap, bitmap,
                                                 &black, &black, 0, 0);
      p_XFreePixmap(host->display, bitmap);
    }
  } else {
    unsigned int code = XC_left_ptr;
    switch (shape) {
      case 1: code = XC_hand2; break;
      case 2: code = XC_xterm; break;
      case 3: code = XC_crosshair; break;
      case 4: code = XC_fleur; break;
      case 5: code = XC_X_cursor; break;
      case 6: code = XC_watch; break;
      case 7: code = XC_watch; break;
      case 8: code = XC_question_arrow; break;
      case 9: code = XC_sb_v_double_arrow; break;
      case 10: code = XC_sb_h_double_arrow; break;
      case 11: code = XC_bottom_left_corner; break;
      case 12: code = XC_bottom_right_corner; break;
      case 13: code = XC_hand1; break;
      case 14: code = XC_hand1; break;
      case 15: code = XC_tcross; break;
      case 16: code = XC_crosshair; break;
      case 17: code = XC_fleur; break;
      case 18: code = XC_sb_h_double_arrow; break;
      case 19: code = XC_sb_v_double_arrow; break;
      default: code = XC_left_ptr; break;
    }
    win->cursor_handle = p_XCreateFontCursor(host->display, code);
  }
  if (win->cursor_handle != None) {
    p_XDefineCursor(host->display, win->window, win->cursor_handle);
  }
}

/* ------------------------------------------------------------------------- */
/* Clipboard                                                                  */
/* ------------------------------------------------------------------------- */

static void clipboard_own(moonapp_host *host, moonapp_win *win) {
  if (win == NULL) return;
  p_XSetSelectionOwner(host->display, host->clipboard, win->window, CurrentTime);
  p_XFlush(host->display);
}

static void serve_selection(moonapp_host *host, XSelectionRequestEvent *request) {
  XSelectionEvent reply;
  memset(&reply, 0, sizeof(reply));
  reply.type = SelectionNotify;
  reply.display = request->display;
  reply.requestor = request->requestor;
  reply.selection = request->selection;
  reply.target = request->target;
  reply.time = request->time;
  reply.property = None;
  if (request->target == host->targets) {
    Atom offered[2] = { host->utf8_string, XA_STRING };
    p_XChangeProperty(host->display, request->requestor, request->property,
                      XA_ATOM, 32, PropModeReplace, (unsigned char *)offered, 2);
    reply.property = request->property;
  } else if (request->target == host->utf8_string ||
             request->target == XA_STRING) {
    p_XChangeProperty(host->display, request->requestor, request->property,
                      request->target, 8, PropModeReplace,
                      (unsigned char *)host->clipboard_text,
                      host->clipboard_len);
    reply.property = request->property;
  }
  p_XSendEvent(host->display, request->requestor, False, NoEventMask,
               (XEvent *)&reply);
  p_XFlush(host->display);
}

/* ------------------------------------------------------------------------- */
/* Events                                                                     */
/* ------------------------------------------------------------------------- */

static void emit_text(moonapp_host *host, moonapp_win *win, const char *text,
                      int32_t len) {
  if (len <= 0) return;
  moonapp_event *event = push_event(host, MOONAPP_EV_TEXT, win->id);
  if (event == NULL) return;
  moonapp_event_set_text(event, text, len);
}

static void handle_event(moonapp_host *host, XEvent *event) {
  moonapp_win *win = window_of(host, event->xany.window);
  switch (event->type) {
    case KeyPress:
    case KeyRelease: {
      if (win == NULL) return;
      if (p_XFilterEvent(event, win->window)) return;
      moonapp_event *pressed =
          push_event(host, event->type == KeyPress ? MOONAPP_EV_KEY_DOWN
                                                  : MOONAPP_EV_KEY_UP,
                     win->id);
      if (pressed != NULL) {
        KeySym keysym = p_XLookupKeysym(&event->xkey, 0);
        pressed->i32a = (int32_t)keysym;
        pressed->i32b = modifiers_of(event->xkey.state);
        pressed->i32c = 0;
        pressed->i32d = (int32_t)event->xkey.keycode;
      }
      if (event->type == KeyPress) {
        char text[64];
        int32_t len = 0;
        if (win->ic != NULL) {
          KeySym ignored = 0;
          Status status = 0;
          len = p_Xutf8LookupString(win->ic, &event->xkey, text,
                                    (int)sizeof(text) - 1, &ignored, &status);
          if (status != XLookupChars && status != XLookupBoth) len = 0;
        }
        if (len <= 0) {
          /* No input method had anything to say about this key, so the
           * keyboard layout's own mapping is the answer. */
          KeySym ignored = 0;
          len = p_XLookupString(&event->xkey, text, (int)sizeof(text) - 1,
                                &ignored, NULL);
        }
        /* Control characters are keys: the Enter key produces a newline the
         * application asks for itself. */
        if (len > 0 && (unsigned char)text[0] >= 0x20 &&
            (unsigned char)text[0] != 0x7F) {
          emit_text(host, win, text, len);
        }
      }
      return;
    }
    case ButtonPress:
    case ButtonRelease: {
      if (win == NULL) return;
      unsigned int button = event->xbutton.button;
      if (button >= 4 && button <= 7) {
        if (event->type != ButtonPress) return;
        moonapp_event *wheel = push_event(host, MOONAPP_EV_WHEEL, win->id);
        if (wheel != NULL) {
          wheel->i32a = 0;
          wheel->f64a = button == 6 ? -1.0 : (button == 7 ? 1.0 : 0.0);
          wheel->f64b = button == 4 ? 1.0 : (button == 5 ? -1.0 : 0.0);
        }
        return;
      }
      int32_t code = 0;
      switch (button) {
        case 1: code = 0; break;
        case 2: code = 2; break;
        case 3: code = 1; break;
        case 8: code = 3; break;
        case 9: code = 4; break;
        default: code = 5 + (int32_t)button; break;
      }
      moonapp_event *pressed =
          push_event(host, event->type == ButtonPress ? MOONAPP_EV_BUTTON_DOWN
                                                      : MOONAPP_EV_BUTTON_UP,
                     win->id);
      if (pressed != NULL) {
        pressed->i32a = code;
        pressed->i32b = modifiers_of(event->xbutton.state);
      }
      return;
    }
    case MotionNotify: {
      if (win == NULL) return;
      moonapp_event *moved = push_event(host, MOONAPP_EV_POINTER_MOVED, win->id);
      if (moved != NULL) {
        moved->f64a = (double)event->xmotion.x;
        moved->f64b = (double)event->xmotion.y;
      }
      return;
    }
    case EnterNotify:
      if (win != NULL) push_event(host, MOONAPP_EV_POINTER_ENTERED, win->id);
      return;
    case LeaveNotify:
      if (win != NULL) push_event(host, MOONAPP_EV_POINTER_LEFT, win->id);
      return;
    case FocusIn:
      if (win != NULL) {
        moonapp_event *focused = push_event(host, MOONAPP_EV_FOCUSED, win->id);
        if (focused != NULL) focused->i32a = 1;
        if (win->ic != NULL) p_XSetICFocus(win->ic);
      }
      return;
    case FocusOut:
      if (win != NULL) {
        moonapp_event *focused = push_event(host, MOONAPP_EV_FOCUSED, win->id);
        if (focused != NULL) focused->i32a = 0;
        if (win->ic != NULL) p_XUnsetICFocus(win->ic);
      }
      return;
    case ConfigureNotify: {
      if (win == NULL) return;
      int32_t width = event->xconfigure.width;
      int32_t height = event->xconfigure.height;
      if (width == win->width && height == win->height) return;
      win->width = width;
      win->height = height;
      moonapp_event *resized = push_event(host, MOONAPP_EV_RESIZED, win->id);
      if (resized != NULL) {
        resized->i32a = width;
        resized->i32b = height;
        resized->f64a = win->scale;
      }
      return;
    }
    case ClientMessage:
      if (win != NULL && (Atom)event->xclient.data.l[0] == host->wm_delete) {
        push_event(host, MOONAPP_EV_CLOSE, win->id);
      }
      return;
    case SelectionRequest:
      serve_selection(host, &event->xselectionrequest);
      return;
    default:
      return;
  }
}

/* ------------------------------------------------------------------------- */
/* Host                                                                       */
/* ------------------------------------------------------------------------- */

static void release_window(moonapp_win *win) {
  moonapp_host *host = win->host;
  if (win->image != NULL) {
    /* XDestroyImage frees the pixel data it was given. */
    p_XDestroyImage(win->image);
    win->image = NULL;
    win->image_data = NULL;
  }
  if (win->gc != NULL) {
    p_XFreeGC(host->display, win->gc);
    win->gc = NULL;
  }
  if (win->cursor_handle != None) {
    p_XFreeCursor(host->display, win->cursor_handle);
    win->cursor_handle = None;
  }
  if (win->ic != NULL) {
    p_XDestroyIC(win->ic);
    win->ic = NULL;
  }
  if (win->window != None && win->alive) {
    p_XDestroyWindow(host->display, win->window);
  }
  win->window = None;
  win->alive = 0;
}

MOONBIT_FFI_EXPORT
moonapp_host *x11_host_new(void) {
  if (!load_x11()) return NULL;
  Display *display = p_XOpenDisplay(NULL);
  if (display == NULL) return NULL;
  moonapp_host *host = (moonapp_host *)calloc(1, sizeof(moonapp_host));
  if (host == NULL) {
    p_XCloseDisplay(display);
    return NULL;
  }
  moonapp_queue_init(&host->queue);
  host->next_id = 1;
  host->display = display;
  host->screen = DefaultScreen(display);
  host->root = RootWindow(display, host->screen);
  host->visual = DefaultVisual(display, host->screen);
  host->depth = DefaultDepth(display, host->screen);
  host->wm_delete = p_XInternAtom(display, "WM_DELETE_WINDOW", False);
  host->net_wm_name = p_XInternAtom(display, "_NET_WM_NAME", False);
  host->utf8_string = p_XInternAtom(display, "UTF8_STRING", False);
  host->clipboard = p_XInternAtom(display, "CLIPBOARD", False);
  host->property = p_XInternAtom(display, "MOONAPP_SELECTION", False);
  host->targets = p_XInternAtom(display, "TARGETS", False);
  host->im = p_XOpenIM(display, NULL, NULL, NULL);
  return host;
}

MOONBIT_FFI_EXPORT
int32_t x11_host_ok(moonapp_host *host) { return host != NULL ? 1 : 0; }

MOONBIT_FFI_EXPORT
int32_t x11_host_error(moonapp_host *host, char *into, int32_t cap) {
  const char *message = host == NULL ? "no display to connect to" : host->error;
  if (into == NULL || cap <= 0) return 0;
  int32_t len = (int32_t)strlen(message);
  if (len > cap - 1) len = cap - 1;
  memcpy(into, message, (size_t)len);
  into[len] = 0;
  return len;
}

MOONBIT_FFI_EXPORT
void x11_host_free(moonapp_host *host) {
  if (host == NULL) return;
  for (int32_t i = 0; i < host->count; i++) {
    if (host->windows[i].alive) release_window(&host->windows[i]);
  }
  free(host->clipboard_text);
  if (host->im != NULL) p_XCloseIM(host->im);
  p_XCloseDisplay(host->display);
  free(host);
}

MOONBIT_FFI_EXPORT
double x11_primary_scale(moonapp_host *host) {
  if (host == NULL) return 1.0;
  return read_scale(host);
}

MOONBIT_FFI_EXPORT
int32_t x11_is_shutdown(moonapp_host *host) {
  return host == NULL ? 1 : host->quitting;
}

MOONBIT_FFI_EXPORT
void x11_host_shutdown(moonapp_host *host) {
  if (host != NULL) host->quitting = 1;
}

MOONBIT_FFI_EXPORT
int32_t x11_open(moonapp_host *host, const char *title, int32_t title_len,
                     const char *app_id, int32_t app_id_len, int32_t width,
                     int32_t height, int32_t flags) {
  if (host == NULL) return -1;
  if (host->count >= MOONAPP_MAX_WINDOWS) {
    moonapp_error(host->error, "no room for another window");
    return -1;
  }
  Display *display = host->display;
  XSetWindowAttributes attributes;
  memset(&attributes, 0, sizeof(attributes));
  attributes.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask |
                          KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                          PointerMotionMask | FocusChangeMask |
                          PropertyChangeMask | EnterWindowMask |
                          LeaveWindowMask;
  attributes.background_pixel = BlackPixel(display, host->screen);
  attributes.border_pixel = 0;
  attributes.colormap = DefaultColormap(display, host->screen);
  unsigned long mask = CWEventMask | CWBackPixel | CWBorderPixel | CWColormap;
  Window window = p_XCreateWindow(
      display, host->root, 0, 0, (unsigned int)(width > 0 ? width : 1),
      (unsigned int)(height > 0 ? height : 1), 0, host->depth, InputOutput,
      host->visual, mask, &attributes);
  if (window == None) {
    moonapp_error(host->error, "the display refused the window");
    return -1;
  }
  moonapp_win *win = &host->windows[host->count];
  memset(win, 0, sizeof(*win));
  win->host = host;
  win->id = host->next_id++;
  win->alive = 1;
  win->window = window;
  win->width = width > 0 ? width : 1;
  win->height = height > 0 ? height : 1;
  win->scale = read_scale(host);
  win->ime_allowed = 1;
  win->gc = p_XCreateGC(display, window, 0, NULL);
  p_XSetWMProtocols(display, window, &host->wm_delete, 1);
  /* The title goes in as UTF-8 under its own property, and again as the name
   * the oldest clients read. */
  p_XChangeProperty(display, window, host->net_wm_name, host->utf8_string, 8,
                    PropModeReplace, (const unsigned char *)title, title_len);
  char *name = (char *)malloc((size_t)title_len + 1);
  if (name != NULL) {
    memcpy(name, title, (size_t)title_len);
    name[title_len] = 0;
    p_XStoreName(display, window, name);
    free(name);
  }
  if (app_id_len > 0) {
    char *instance = (char *)malloc((size_t)app_id_len + 1);
    char *klass = (char *)malloc((size_t)app_id_len + 1);
    if (instance != NULL && klass != NULL) {
      memcpy(instance, app_id, (size_t)app_id_len);
      instance[app_id_len] = 0;
      memcpy(klass, app_id, (size_t)app_id_len);
      klass[app_id_len] = 0;
      XClassHint hint;
      hint.res_name = instance;
      hint.res_class = klass;
      p_XSetClassHint(display, window, &hint);
    }
    free(instance);
    free(klass);
  }
  XSizeHints hints;
  memset(&hints, 0, sizeof(hints));
  hints.flags = PSize;
  hints.width = win->width;
  hints.height = win->height;
  if (!(flags & MOONAPP_FLAG_RESIZABLE)) {
    hints.flags |= PMinSize | PMaxSize;
    hints.min_width = win->width;
    hints.max_width = win->width;
    hints.min_height = win->height;
    hints.max_height = win->height;
  }
  p_XSetWMNormalHints(display, window, &hints);
  if (host->im != NULL) {
    win->ic = p_XCreateIC(host->im, XNInputStyle,
                          XIMPreeditNothing | XIMStatusNothing, XNClientWindow,
                          window, XNFocusWindow, window, NULL);
  }
  host->count++;
  if (flags & MOONAPP_FLAG_VISIBLE) {
    p_XMapWindow(display, window);
  }
  p_XFlush(display);
  return win->id;
}

MOONBIT_FFI_EXPORT
void x11_close(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL) return;
  release_window(win);
  p_XFlush(host->display);
}

MOONBIT_FFI_EXPORT
int32_t x11_window_open(moonapp_host *host, int32_t id) {
  return find_window(host, id) != NULL;
}

static int32_t pump(moonapp_host *host, int32_t timeout_ms, int32_t wait) {
  if (host->has_current) return 1;
  if (moonapp_queue_pop(&host->queue, &host->current)) {
    host->has_current = 1;
    return 1;
  }
  if (wait) {
    if (p_XPending(host->display) == 0) {
      fd_set readable;
      FD_ZERO(&readable);
      FD_SET(ConnectionNumber(host->display), &readable);
      struct timeval timeout;
      timeout.tv_sec = timeout_ms < 0 ? 3600 : timeout_ms / 1000;
      timeout.tv_usec = timeout_ms < 0 ? 0 : (timeout_ms % 1000) * 1000;
      select(FD_SETSIZE, &readable, NULL, NULL, &timeout);
    }
  }
  while (p_XPending(host->display) > 0) {
    XEvent event;
    p_XNextEvent(host->display, &event);
    handle_event(host, &event);
    if (moonapp_queue_pop(&host->queue, &host->current)) {
      host->has_current = 1;
      return 1;
    }
  }
  return 0;
}

MOONBIT_FFI_EXPORT
int32_t x11_poll(moonapp_host *host) {
  if (host == NULL) return 0;
  return pump(host, 0, 0);
}

MOONBIT_FFI_EXPORT
int32_t x11_wait(moonapp_host *host, int32_t timeout_ms) {
  if (host == NULL) return 0;
  return pump(host, timeout_ms, 1);
}

MOONBIT_FFI_EXPORT
void x11_event_release(moonapp_host *host) {
  if (host != NULL) host->has_current = 0;
}

MOONBIT_FFI_EXPORT
int32_t x11_event_kind(moonapp_host *host) { return host->current.kind; }

MOONBIT_FFI_EXPORT
int32_t x11_event_window(moonapp_host *host) { return host->current.window; }

MOONBIT_FFI_EXPORT
int32_t x11_event_i32(moonapp_host *host, int32_t which) {
  switch (which) {
    case 0: return host->current.i32a;
    case 1: return host->current.i32b;
    case 2: return host->current.i32c;
    default: return host->current.i32d;
  }
}

MOONBIT_FFI_EXPORT
double x11_event_f64(moonapp_host *host, int32_t which) {
  return which == 0 ? host->current.f64a : host->current.f64b;
}

MOONBIT_FFI_EXPORT
int32_t x11_event_text_len(moonapp_host *host) {
  return host->current.text_len;
}

MOONBIT_FFI_EXPORT
int32_t x11_event_text(moonapp_host *host, uint8_t *buffer, int32_t cap) {
  int32_t len = host->current.text_len;
  if (buffer == NULL || cap < len) return len;
  memcpy(buffer, host->current.text, (size_t)len);
  return len;
}

/* ------------------------------------------------------------------------- */
/* Window properties                                                          */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
void x11_set_title(moonapp_host *host, int32_t id, const char *title,
                       int32_t len) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL) return;
  p_XChangeProperty(host->display, win->window, host->net_wm_name,
                    host->utf8_string, 8, PropModeReplace,
                    (const unsigned char *)title, len);
  char *name = (char *)malloc((size_t)len + 1);
  if (name != NULL) {
    memcpy(name, title, (size_t)len);
    name[len] = 0;
    p_XStoreName(host->display, win->window, name);
    free(name);
  }
  p_XFlush(host->display);
}

MOONBIT_FFI_EXPORT
void x11_set_size(moonapp_host *host, int32_t id, int32_t width,
                      int32_t height) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL) return;
  p_XResizeWindow(host->display, win->window,
                  (unsigned int)(width > 0 ? width : 1),
                  (unsigned int)(height > 0 ? height : 1));
  p_XFlush(host->display);
}

MOONBIT_FFI_EXPORT
void x11_set_min_size(moonapp_host *host, int32_t id, int32_t width,
                          int32_t height) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL) return;
  XSizeHints hints;
  memset(&hints, 0, sizeof(hints));
  hints.flags = PMinSize;
  hints.min_width = width;
  hints.min_height = height;
  p_XSetWMNormalHints(host->display, win->window, &hints);
  p_XFlush(host->display);
}

MOONBIT_FFI_EXPORT
int32_t x11_width(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  return win == NULL ? 0 : win->width;
}

MOONBIT_FFI_EXPORT
int32_t x11_height(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  return win == NULL ? 0 : win->height;
}

MOONBIT_FFI_EXPORT
double x11_scale(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  return win == NULL ? 1.0 : win->scale;
}

MOONBIT_FFI_EXPORT
void x11_set_cursor(moonapp_host *host, int32_t id, int32_t cursor) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL) return;
  win->cursor = cursor;
  apply_cursor(win, cursor);
  p_XFlush(host->display);
}

/* ------------------------------------------------------------------------- */
/* Frames                                                                     */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
int32_t x11_present(moonapp_host *host, int32_t id, uint8_t *bgra,
                        int32_t width, int32_t height, int32_t stride) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL || bgra == NULL) return 0;
  if (width <= 0 || height <= 0) return 0;
  if (win->image == NULL || (int32_t)win->image->width != width ||
      (int32_t)win->image->height != height) {
    if (win->image != NULL) {
      p_XDestroyImage(win->image);
      win->image = NULL;
    }
    win->image_data = (char *)calloc((size_t)width * (size_t)height, 4);
    if (win->image_data == NULL) return 0;
    win->image = p_XCreateImage(host->display, host->visual, (unsigned int)24,
                                ZPixmap, 0, win->image_data, (unsigned int)width,
                                (unsigned int)height, 32, 0);
    if (win->image == NULL) {
      free(win->image_data);
      win->image_data = NULL;
      return 0;
    }
  }
  int32_t row = width * 4;
  for (int32_t y = 0; y < height; y++) {
    memcpy(win->image->data + (size_t)y * (size_t)win->image->bytes_per_line,
           bgra + (size_t)y * (size_t)stride, (size_t)row);
  }
  p_XPutImage(host->display, win->window, win->gc, win->image, 0, 0, 0, 0,
              (unsigned int)width, (unsigned int)height);
  p_XFlush(host->display);
  return 1;
}

MOONBIT_FFI_EXPORT
int32_t x11_read_back(moonapp_host *host, int32_t id, uint8_t *out,
                          int32_t cap) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL || win->window == None) return -1;
  int32_t width = win->width;
  int32_t height = win->height;
  if (width <= 0 || height <= 0) return -1;
  int32_t need = width * height * 4;
  if (out == NULL || cap < need) return need;
  XImage *image = p_XGetImage(host->display, win->window, 0, 0,
                              (unsigned int)width, (unsigned int)height,
                              AllPlanes, ZPixmap);
  if (image == NULL) return -1;
  for (int32_t y = 0; y < height; y++) {
    const uint8_t *from =
        (const uint8_t *)image->data + (size_t)y * (size_t)image->bytes_per_line;
    uint8_t *to = out + (size_t)y * (size_t)width * 4;
    for (int32_t x = 0; x < width; x++) {
      uint32_t pixel = 0;
      memcpy(&pixel, from + (size_t)x * 4, 4);
      /* The server reports the colour channels; a window is opaque, so the
       * alpha a caller sees is the one every window has. */
      to[x * 4] = (uint8_t)(pixel & 0xFF);
      to[x * 4 + 1] = (uint8_t)((pixel >> 8) & 0xFF);
      to[x * 4 + 2] = (uint8_t)((pixel >> 16) & 0xFF);
      to[x * 4 + 3] = 0xFF;
    }
  }
  p_XDestroyImage(image);
  return need;
}

/* ------------------------------------------------------------------------- */
/* Clipboard                                                                  */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
int32_t x11_clipboard_get(moonapp_host *host, uint8_t *buffer, int32_t cap) {
  if (host == NULL) return -1;
  /* Reading from ourselves is a copy, not a round trip through the server. */
  if (p_XGetSelectionOwner(host->display, host->clipboard) != None) {
    moonapp_win *owner = NULL;
    for (int32_t i = 0; i < host->count; i++) {
      if (host->windows[i].alive &&
          host->windows[i].window ==
              p_XGetSelectionOwner(host->display, host->clipboard)) {
        owner = &host->windows[i];
      }
    }
    if (owner != NULL) {
      if (buffer == NULL || cap < host->clipboard_len) return host->clipboard_len;
      memcpy(buffer, host->clipboard_text, (size_t)host->clipboard_len);
      return host->clipboard_len;
    }
  }
  moonapp_win *win = NULL;
  for (int32_t i = 0; i < host->count; i++) {
    if (host->windows[i].alive) {
      win = &host->windows[i];
      break;
    }
  }
  if (win == NULL) return -1;
  p_XDeleteProperty(host->display, win->window, host->property);
  p_XConvertSelection(host->display, host->clipboard, host->utf8_string,
                      host->property, win->window, CurrentTime);
  p_XFlush(host->display);
  /* The owner answers when it gets round to it; two seconds is longer than any
   * desktop takes and short enough not to look like a hang. */
  for (int32_t attempt = 0; attempt < 200; attempt++) {
    while (p_XPending(host->display) > 0) {
      XEvent event;
      p_XNextEvent(host->display, &event);
      if (event.type == SelectionNotify &&
          event.xselection.requestor == win->window) {
        goto answered;
      }
      handle_event(host, &event);
    }
    usleep(10000);
  }
answered:
  {
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long rest = 0;
    unsigned char *data = NULL;
    int32_t len = -1;
    if (p_XGetWindowProperty(host->display, win->window, host->property, 0, 65536,
                             True, AnyPropertyType, &type, &format, &count,
                             &rest, &data) == Success &&
        data != NULL) {
      len = (int32_t)count;
      if (buffer != NULL && cap >= len) memcpy(buffer, data, (size_t)len);
      p_XFree(data);
    }
    p_XDeleteProperty(host->display, win->window, host->property);
    return len;
  }
}

MOONBIT_FFI_EXPORT
void x11_clipboard_set(moonapp_host *host, const char *text, int32_t len) {
  if (host == NULL) return;
  moonapp_win *win = NULL;
  for (int32_t i = 0; i < host->count; i++) {
    if (host->windows[i].alive) {
      win = &host->windows[i];
      break;
    }
  }
  if (win == NULL) return;
  free(host->clipboard_text);
  host->clipboard_text = (char *)malloc((size_t)len + 1);
  if (host->clipboard_text == NULL) {
    host->clipboard_len = 0;
    return;
  }
  memcpy(host->clipboard_text, text, (size_t)len);
  host->clipboard_text[len] = 0;
  host->clipboard_len = len;
  clipboard_own(host, win);
}

/* ------------------------------------------------------------------------- */
/* Input methods                                                              */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
void x11_set_ime_allowed(moonapp_host *host, int32_t id, int32_t allowed) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL) return;
  win->ime_allowed = allowed;
  if (allowed) {
    if (win->ic == NULL && host->im != NULL) {
      win->ic = p_XCreateIC(host->im, XNInputStyle,
                            XIMPreeditNothing | XIMStatusNothing, XNClientWindow,
                            win->window, XNFocusWindow, win->window, NULL);
    }
  } else if (win->ic != NULL) {
    p_XDestroyIC(win->ic);
    win->ic = NULL;
  }
}

MOONBIT_FFI_EXPORT
void x11_set_ime_area(moonapp_host *host, int32_t id, int32_t x, int32_t y,
                          int32_t width, int32_t height) {
  /* XIM asks the input method server where the caret is through a spot that
   * only a preedit-aware style carries; the backend uses XIMPreeditNothing, so
   * the candidate window belongs to the server. */
  (void)host;
  (void)id;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
}

/* ------------------------------------------------------------------------- */
/* Native handles, for a GPU surface                                          */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
int32_t x11_native_kind(void) { return MOONAPP_PLATFORM_X11; }

MOONBIT_FFI_EXPORT
uint64_t x11_native_display(moonapp_host *host) {
  return host == NULL ? 0 : (uint64_t)(uintptr_t)host->display;
}

MOONBIT_FFI_EXPORT
uint64_t x11_native_window(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  return win == NULL ? 0 : (uint64_t)win->window;
}

/* ------------------------------------------------------------------------- */
/* Test hooks                                                                 */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
int32_t x11_test_key(moonapp_host *host, int32_t id, int32_t code,
                         int32_t down) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL) return 0;
  XEvent event;
  memset(&event, 0, sizeof(event));
  event.xkey.type = down ? KeyPress : KeyRelease;
  event.xkey.display = host->display;
  event.xkey.window = win->window;
  event.xkey.root = host->root;
  event.xkey.time = CurrentTime;
  event.xkey.keycode = p_XKeysymToKeycode(host->display, (KeySym)code);
  event.xkey.same_screen = True;
  p_XSendEvent(host->display, win->window, True, down ? KeyPressMask
                                                      : KeyReleaseMask,
               &event);
  p_XFlush(host->display);
  return 1;
}

MOONBIT_FFI_EXPORT
int32_t x11_test_text(moonapp_host *host, int32_t id, int32_t unit) {
  /* X11 has no message that inserts text on its own: text comes from a key
   * press through the input context, which is what the key hook drives. */
  (void)host;
  (void)id;
  (void)unit;
  return 0;
}

MOONBIT_FFI_EXPORT
int32_t x11_test_pointer(moonapp_host *host, int32_t id, int32_t kind,
                             int32_t x, int32_t y, int32_t button) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL) return 0;
  XEvent event;
  memset(&event, 0, sizeof(event));
  if (kind == 5) {
    event.xbutton.type = ButtonPress;
    event.xbutton.display = host->display;
    event.xbutton.window = win->window;
    event.xbutton.root = host->root;
    event.xbutton.x = x;
    event.xbutton.y = y;
    event.xbutton.button = y >= 0 ? 4 : 5;
    p_XSendEvent(host->display, win->window, True, ButtonPressMask, &event);
    p_XFlush(host->display);
    return 1;
  }
  if (kind == 0) {
    event.xmotion.type = MotionNotify;
    event.xmotion.display = host->display;
    event.xmotion.window = win->window;
    event.xmotion.root = host->root;
    event.xmotion.x = x;
    event.xmotion.y = y;
    event.xmotion.same_screen = True;
    p_XSendEvent(host->display, win->window, True, PointerMotionMask, &event);
  } else {
    event.xbutton.type = kind == 1 || kind == 3 ? ButtonPress : ButtonRelease;
    event.xbutton.display = host->display;
    event.xbutton.window = win->window;
    event.xbutton.root = host->root;
    event.xbutton.x = x;
    event.xbutton.y = y;
    event.xbutton.button = kind == 1 || kind == 2 ? 1 : 3;
    p_XSendEvent(host->display, win->window, True,
                 event.xbutton.type == ButtonPress ? ButtonPressMask
                                                   : ButtonReleaseMask,
                 &event);
  }
  p_XFlush(host->display);
  return 1;
}

#endif /* __linux__ */
