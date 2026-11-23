/*
 * Win32 backend: a window class per application name, a DIB section as the
 * frame buffer, and the desktop's own message queue as the event source.
 *
 * Everything the desktop provides is fetched with GetProcAddress from libraries
 * that are always loaded, so importing this module adds no link flag to a
 * consumer's build.
 */

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <imm.h>
#include <moonbit.h>

/* Drawing is GDI, which no default link line carries. The directive travels in
 * this object, so a consumer's build needs no flag of its own. */
#if defined(_MSC_VER)
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#endif

#include "host_internal.h"

typedef struct moonapp_win {
  int32_t id;
  int32_t alive;
  HWND hwnd;
  struct moonapp_host *host;
  int32_t width;
  int32_t height;
  double scale;
  int32_t cursor;
  int32_t ime_allowed;
  HCURSOR cursor_handle;
  HDC memory_dc;
  HBITMAP dib;
  void *dib_bits;
  int32_t dib_width;
  int32_t dib_height;
  int32_t tracking_leave;
  int32_t gesture_distance;
  int32_t gesture_active;
} moonapp_win;

typedef struct moonapp_host {
  moonapp_queue queue;
  moonapp_event current;
  int32_t has_current;
  int32_t next_id;
  moonapp_win windows[MOONAPP_MAX_WINDOWS];
  int32_t count;
  char error[MOONAPP_ERROR];
  HINSTANCE instance;
  int32_t dpi_aware;
  int32_t quitting;
} moonapp_host;

/* Resolved from the libraries the process already has, so no link flag is
 * needed: the two DPI entry points are Windows 10 additions and the input method
 * ones live in imm32. */
typedef BOOL(WINAPI *moonapp_get_dpi_for_window)(HWND);
typedef BOOL(WINAPI *moonapp_adjust_window_rect_ex_for_dpi)(LPRECT, DWORD, BOOL,
                                                            DWORD, UINT);
typedef BOOL(WINAPI *moonapp_set_dpi_awareness_context)(HANDLE);
typedef HIMC(WINAPI *moonapp_imm_get_context)(HWND);
typedef BOOL(WINAPI *moonapp_imm_release_context)(HWND, HIMC);
typedef BOOL(WINAPI *moonapp_imm_set_composition_window)(HIMC,
                                                         LPCOMPOSITIONFORM);
typedef HIMC(WINAPI *moonapp_imm_associate_context)(HWND, HIMC);
typedef LONG(WINAPI *moonapp_imm_get_composition_string)(HIMC, DWORD, LPVOID,
                                                         DWORD);
typedef BOOL(WINAPI *moonapp_imm_associate_context_ex)(HWND, HIMC, DWORD);

static moonapp_get_dpi_for_window p_get_dpi_for_window;
static moonapp_adjust_window_rect_ex_for_dpi p_adjust_window_rect_ex_for_dpi;
static moonapp_set_dpi_awareness_context p_set_dpi_awareness_context;
static moonapp_imm_get_context p_imm_get_context;
static moonapp_imm_release_context p_imm_release_context;
static moonapp_imm_set_composition_window p_imm_set_composition_window;
static moonapp_imm_associate_context p_imm_associate_context;
static moonapp_imm_get_composition_string p_imm_get_composition_string;
static moonapp_imm_associate_context_ex p_imm_associate_context_ex;

static void load_entry_points(void) {
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  HMODULE imm32 = LoadLibraryW(L"imm32.dll");
  if (user32 != NULL) {
    p_get_dpi_for_window =
        (moonapp_get_dpi_for_window)(void *)GetProcAddress(
            user32, "GetDpiForWindow");
    p_adjust_window_rect_ex_for_dpi =
        (moonapp_adjust_window_rect_ex_for_dpi)(void *)GetProcAddress(
            user32, "AdjustWindowRectExForDpi");
    p_set_dpi_awareness_context =
        (moonapp_set_dpi_awareness_context)(void *)GetProcAddress(
            user32, "SetProcessDpiAwarenessContext");
  }
  if (imm32 != NULL) {
    p_imm_get_context = (moonapp_imm_get_context)(void *)GetProcAddress(
        imm32, "ImmGetContext");
    p_imm_release_context = (moonapp_imm_release_context)(void *)GetProcAddress(
        imm32, "ImmReleaseContext");
    p_imm_set_composition_window =
        (moonapp_imm_set_composition_window)(void *)GetProcAddress(
            imm32, "ImmSetCompositionWindow");
    p_imm_associate_context =
        (moonapp_imm_associate_context)(void *)GetProcAddress(
            imm32, "ImmAssociateContext");
    p_imm_get_composition_string =
        (moonapp_imm_get_composition_string)(void *)GetProcAddress(
            imm32, "ImmGetCompositionStringW");
    p_imm_associate_context_ex =
        (moonapp_imm_associate_context_ex)(void *)GetProcAddress(
            imm32, "ImmAssociateContextEx");
  }
}

/* ------------------------------------------------------------------------- */
/* UTF-8 and UTF-16                                                           */
/* ------------------------------------------------------------------------- */

static WCHAR *widen(const char *text, int32_t len) {
  if (text == NULL || len <= 0) {
    WCHAR *empty = (WCHAR *)calloc(1, sizeof(WCHAR));
    return empty;
  }
  int32_t size = MultiByteToWideChar(CP_UTF8, 0, text, len, NULL, 0);
  if (size <= 0) size = 0;
  WCHAR *wide = (WCHAR *)calloc((size_t)size + 1, sizeof(WCHAR));
  if (wide == NULL) return NULL;
  if (size > 0) MultiByteToWideChar(CP_UTF8, 0, text, len, wide, size);
  wide[size] = 0;
  return wide;
}

static void fill_utf16(moonapp_event *event, const WCHAR *wide,
                      int32_t units) {
  if (wide == NULL || units <= 0) return;
  int32_t size = WideCharToMultiByte(CP_UTF8, 0, wide, units, NULL, 0, NULL,
                                     NULL);
  if (size <= 0) return;
  char *utf8 = (char *)malloc((size_t)size + 1);
  if (utf8 == NULL) return;
  WideCharToMultiByte(CP_UTF8, 0, wide, units, utf8, size, NULL, NULL);
  utf8[size] = 0;
  moonapp_event_set_text(event, utf8, size);
  free(utf8);
}

static void emit_utf16(moonapp_host *host, int32_t kind, int32_t window,
                       const WCHAR *wide, int32_t units) {
  moonapp_event *event = moonapp_queue_push(&host->queue);
  if (event == NULL) return;
  event->kind = kind;
  event->window = window;
  fill_utf16(event, wide, units);
}

/* ------------------------------------------------------------------------- */
/* Frames                                                                     */
/* ------------------------------------------------------------------------- */

static void release_frame(moonapp_win *win) {
  if (win->dib != NULL) {
    DeleteObject(win->dib);
    win->dib = NULL;
    win->dib_bits = NULL;
  }
  if (win->memory_dc != NULL) {
    DeleteDC(win->memory_dc);
    win->memory_dc = NULL;
  }
  win->dib_width = 0;
  win->dib_height = 0;
}

static int32_t ensure_frame(moonapp_win *win) {
  if (win->dib != NULL && win->dib_width == win->width &&
      win->dib_height == win->height) {
    return 1;
  }
  release_frame(win);
  if (win->width <= 0 || win->height <= 0) return 0;
  BITMAPINFO info;
  memset(&info, 0, sizeof(info));
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = win->width;
  /* Negative height: the first row of the buffer is the top row of the window,
   * which is the order a frame is in. */
  info.bmiHeader.biHeight = -win->height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  HDC screen = GetDC(NULL);
  win->dib = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &win->dib_bits,
                              NULL, 0);
  if (win->dib != NULL) win->memory_dc = CreateCompatibleDC(screen);
  ReleaseDC(NULL, screen);
  if (win->dib == NULL || win->memory_dc == NULL) {
    release_frame(win);
    return 0;
  }
  SelectObject(win->memory_dc, win->dib);
  win->dib_width = win->width;
  win->dib_height = win->height;
  return 1;
}

static void blit_frame(moonapp_win *win, HDC target) {
  if (win->dib == NULL) return;
  BitBlt(target, 0, 0, win->width, win->height, win->memory_dc, 0, 0, SRCCOPY);
}

/* ------------------------------------------------------------------------- */
/* Cursors                                                                    */
/* ------------------------------------------------------------------------- */

static LPCWSTR cursor_name(int32_t cursor) {
  switch (cursor) {
    case 1: return (LPCWSTR)IDC_HAND;
    case 2: return (LPCWSTR)IDC_IBEAM;
    case 3: return (LPCWSTR)IDC_CROSS;
    case 4: return (LPCWSTR)IDC_SIZEALL;
    case 5: return (LPCWSTR)IDC_NO;
    case 6: return (LPCWSTR)IDC_WAIT;
    case 7: return (LPCWSTR)IDC_APPSTARTING;
    case 8: return (LPCWSTR)IDC_HELP;
    case 9: return (LPCWSTR)IDC_SIZENS;
    case 10: return (LPCWSTR)IDC_SIZEWE;
    case 11: return (LPCWSTR)IDC_SIZENESW;
    case 12: return (LPCWSTR)IDC_SIZENWSE;
    case 13: return (LPCWSTR)IDC_HAND;
    case 14: return (LPCWSTR)IDC_HAND;
    case 15: return (LPCWSTR)IDC_SIZEALL;
    case 16: return (LPCWSTR)IDC_UPARROW;
    case 17: return (LPCWSTR)IDC_SIZEALL;
    case 18: return (LPCWSTR)IDC_SIZEWE;
    case 19: return (LPCWSTR)IDC_SIZENS;
    default: return (LPCWSTR)IDC_ARROW;
  }
}

/* ------------------------------------------------------------------------- */
/* Events                                                                     */
/* ------------------------------------------------------------------------- */

static int32_t modifiers_now(void) {
  int32_t bits = 0;
  if (GetKeyState(VK_SHIFT) & 0x8000) bits |= 1;
  if (GetKeyState(VK_CONTROL) & 0x8000) bits |= 2;
  if (GetKeyState(VK_MENU) & 0x8000) bits |= 4;
  if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) {
    bits |= 8;
  }
  if (GetKeyState(VK_CAPITAL) & 1) bits |= 16;
  if (GetKeyState(VK_NUMLOCK) & 1) bits |= 32;
  return bits;
}

static moonapp_event *push_event(moonapp_host *host, int32_t kind,
                                 int32_t window) {
  moonapp_event *event = moonapp_queue_push(&host->queue);
  if (event == NULL) return NULL;
  event->kind = kind;
  event->window = window;
  return event;
}

static void push_dims(moonapp_host *host, moonapp_win *win) {
  RECT rect;
  GetClientRect(win->hwnd, &rect);
  int32_t width = rect.right - rect.left;
  int32_t height = rect.bottom - rect.top;
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

static void push_scale(moonapp_host *host, moonapp_win *win) {
  if (p_get_dpi_for_window == NULL) return;
  UINT dpi = p_get_dpi_for_window(win->hwnd);
  double scale = 1.0;
  if (dpi > 0) scale = (double)dpi / 96.0;
  if (scale == win->scale) return;
  win->scale = scale;
  moonapp_event *event = push_event(host, MOONAPP_EV_SCALE, win->id);
  if (event != NULL) event->f64a = scale;
}

static void push_key(moonapp_host *host, moonapp_win *win, int32_t kind,
                     WPARAM wparam, LPARAM lparam) {
  moonapp_event *event = push_event(host, kind, win->id);
  if (event == NULL) return;
  event->i32a = (int32_t)wparam;
  event->i32b = modifiers_now();
  /* Bit 30 of the message parameter is set on a repeat, not on a fresh press. */
  event->i32c = (int32_t)((lparam >> 30) & 1);
  event->i32d = (int32_t)(lparam & 0xFFFF);
}

static void push_button(moonapp_host *host, moonapp_win *win, int32_t kind,
                        int32_t button) {
  moonapp_event *event = push_event(host, kind, win->id);
  if (event == NULL) return;
  event->i32a = button;
  event->i32b = modifiers_now();
}

static void push_pointer(moonapp_host *host, moonapp_win *win, LPARAM lparam) {
  moonapp_event *event = push_event(host, MOONAPP_EV_POINTER_MOVED, win->id);
  if (event == NULL) return;
  event->f64a = (double)(int16_t)LOWORD(lparam);
  event->f64b = (double)(int16_t)HIWORD(lparam);
}

/* ------------------------------------------------------------------------- */
/* Window procedure                                                           */
/* ------------------------------------------------------------------------- */

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam,
                                    LPARAM lparam) {
  moonapp_win *win =
      (moonapp_win *)(void *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  if (win == NULL) {
    if (message == WM_NCCREATE) {
      CREATESTRUCTW *create = (CREATESTRUCTW *)(void *)lparam;
      win = (moonapp_win *)create->lpCreateParams;
      win->hwnd = hwnd;
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)(void *)win);
    } else {
      return DefWindowProcW(hwnd, message, wparam, lparam);
    }
  }
  moonapp_host *host = win->host;
  switch (message) {
    case WM_PAINT: {
      PAINTSTRUCT paint;
      HDC dc = BeginPaint(hwnd, &paint);
      if (win->dib != NULL) blit_frame(win, dc);
      EndPaint(hwnd, &paint);
      return 0;
    }
    case WM_ERASEBKGND:
      /* The frame covers the whole client area; erasing first would flash. */
      return 1;
    case WM_PRINTCLIENT:
      blit_frame(win, (HDC)(void *)wparam);
      return 0;
    case WM_SIZE:
      push_dims(host, win);
      return 0;
    case WM_DPICHANGED: {
      push_scale(host, win);
      RECT *suggested = (RECT *)(void *)lparam;
      if (suggested != NULL) {
        SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
      }
      return 0;
    }
    case WM_SETFOCUS: {
      moonapp_event *event = push_event(host, MOONAPP_EV_FOCUSED, win->id);
      if (event != NULL) event->i32a = 1;
      return 0;
    }
    case WM_KILLFOCUS: {
      moonapp_event *event = push_event(host, MOONAPP_EV_FOCUSED, win->id);
      if (event != NULL) event->i32a = 0;
      return 0;
    }
    case WM_CLOSE:
      push_event(host, MOONAPP_EV_CLOSE, win->id);
      return 0;
    case WM_DESTROY:
      win->alive = 0;
      release_frame(win);
      return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      push_key(host, win, MOONAPP_EV_KEY_DOWN, wparam, lparam);
      return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
      push_key(host, win, MOONAPP_EV_KEY_UP, wparam, lparam);
      return 0;
    case WM_CHAR:
    case WM_SYSCHAR: {
      /* Control characters are keys, not text: Enter is a key press, and the
       * text it produces is a newline the application asked for itself. */
      uint32_t unit = (uint32_t)wparam;
      if (unit >= 0x20 && unit != 0x7F) {
        WCHAR text[2];
        text[0] = (WCHAR)unit;
        emit_utf16(host, MOONAPP_EV_TEXT, win->id, text, 1);
      }
      return 0;
    }
    case WM_IME_STARTCOMPOSITION:
      if (win->ime_allowed) {
        push_event(host, MOONAPP_EV_IME_ENABLED, win->id);
      }
      return 0;
    case WM_IME_COMPOSITION: {
      if (p_imm_get_composition_string == NULL) return 0;
      if (!win->ime_allowed) return 0;
      HIMC context = p_imm_get_context(hwnd);
      if (context == NULL) return 0;
      if (lparam & GCS_RESULTSTR) {
        LONG size =
            p_imm_get_composition_string(context, GCS_RESULTSTR, NULL, 0);
        if (size > 0) {
          WCHAR *buffer = (WCHAR *)malloc((size_t)size + sizeof(WCHAR));
          if (buffer != NULL) {
            p_imm_get_composition_string(context, GCS_RESULTSTR, buffer, size);
            emit_utf16(host, MOONAPP_EV_IME_COMMIT, win->id, buffer,
                       size / (LONG)sizeof(WCHAR));
            free(buffer);
          }
        }
      } else if (lparam & GCS_COMPSTR) {
        LONG size =
            p_imm_get_composition_string(context, GCS_COMPSTR, NULL, 0);
        if (size > 0) {
          WCHAR *buffer = (WCHAR *)malloc((size_t)size + sizeof(WCHAR));
          if (buffer != NULL) {
            p_imm_get_composition_string(context, GCS_COMPSTR, buffer, size);
            moonapp_event *event = push_event(host, MOONAPP_EV_IME_PREEDIT,
                                              win->id);
            if (event != NULL) {
              fill_utf16(event, buffer, size / (LONG)sizeof(WCHAR));
              LONG at = p_imm_get_composition_string(context, GCS_CURSORPOS,
                                                    NULL, 0);
              event->i32a = at > 0 ? (int32_t)at : 0;
              event->i32b = event->i32a;
            }
            free(buffer);
          }
        }
      }
      p_imm_release_context(hwnd, context);
      return 0;
    }
    case WM_IME_ENDCOMPOSITION:
      push_event(host, MOONAPP_EV_IME_DISABLED, win->id);
      return 0;
    case WM_MOUSEMOVE: {
      if (!win->tracking_leave) {
        TRACKMOUSEEVENT track;
        memset(&track, 0, sizeof(track));
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = hwnd;
        if (TrackMouseEvent(&track)) {
          win->tracking_leave = 1;
          push_event(host, MOONAPP_EV_POINTER_ENTERED, win->id);
        }
      }
      push_pointer(host, win, lparam);
      return 0;
    }
    case WM_MOUSELEAVE:
      win->tracking_leave = 0;
      push_event(host, MOONAPP_EV_POINTER_LEFT, win->id);
      return 0;
    case WM_LBUTTONDOWN: push_button(host, win, MOONAPP_EV_BUTTON_DOWN, 0); return 0;
    case WM_LBUTTONUP: push_button(host, win, MOONAPP_EV_BUTTON_UP, 0); return 0;
    case WM_RBUTTONDOWN: push_button(host, win, MOONAPP_EV_BUTTON_DOWN, 1); return 0;
    case WM_RBUTTONUP: push_button(host, win, MOONAPP_EV_BUTTON_UP, 1); return 0;
    case WM_MBUTTONDOWN: push_button(host, win, MOONAPP_EV_BUTTON_DOWN, 2); return 0;
    case WM_MBUTTONUP: push_button(host, win, MOONAPP_EV_BUTTON_UP, 2); return 0;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP: {
      int32_t button = 3 + (int32_t)(HIWORD(wparam) == XBUTTON2 ? 1 : 0);
      push_button(host, win,
                  message == WM_XBUTTONDOWN ? MOONAPP_EV_BUTTON_DOWN
                                            : MOONAPP_EV_BUTTON_UP,
                  button);
      return TRUE;
    }
    case WM_MOUSEWHEEL: {
      moonapp_event *event = push_event(host, MOONAPP_EV_WHEEL, win->id);
      if (event != NULL) {
        event->f64a = 0.0;
        /* One notch of a classic wheel is 120 units. */
        event->f64b = (double)(int16_t)HIWORD(wparam) / 120.0;
        event->i32a = 0;
      }
      return 0;
    }
    case WM_GESTURE: {
      GESTUREINFO info;
      memset(&info, 0, sizeof(info));
      info.cbSize = sizeof(info);
      if (!GetGestureInfo((HGESTUREINFO)(void *)lparam, &info)) return 0;
      if (info.dwID == GID_ZOOM) {
        int32_t distance = (int32_t)(info.ullArguments & 0xFFFF);
        if (!win->gesture_active) {
          win->gesture_active = 1;
          win->gesture_distance = distance;
        } else if (win->gesture_distance > 0) {
          double delta = (double)distance / (double)win->gesture_distance;
          win->gesture_distance = distance;
          moonapp_event *event = push_event(host, MOONAPP_EV_GESTURE, win->id);
          if (event != NULL) {
            event->i32a = 0;
            event->f64a = delta;
          }
        }
      } else if (info.dwID == GID_ROTATE) {
        double radians = (double)(int32_t)info.ullArguments / 100.0;
        moonapp_event *event = push_event(host, MOONAPP_EV_GESTURE, win->id);
        if (event != NULL) {
          event->i32a = 1;
          event->f64a = radians * 57.29577951308232;
        }
      }
      CloseGestureInfoHandle((HGESTUREINFO)(void *)lparam);
      return 0;
    }
    case WM_SETCURSOR: {
      if (LOWORD(lparam) == HTCLIENT) {
        SetCursor(win->cursor_handle);
        return TRUE;
      }
      break;
    }
    default:
      break;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

/* ------------------------------------------------------------------------- */
/* Host                                                                       */
/* ------------------------------------------------------------------------- */

static void set_dpi_aware(moonapp_host *host) {
  if (host->dpi_aware) return;
  host->dpi_aware = 1;
  if (p_set_dpi_awareness_context != NULL) {
    /* Per-monitor v2: the window gets WM_DPICHANGED and its own scale factor. */
    if (p_set_dpi_awareness_context((HANDLE)-4)) return;
  }
  SetProcessDPIAware();
}

MOONBIT_FFI_EXPORT
moonapp_host *moonapp_host_new(void) {
  load_entry_points();
  moonapp_host *host = (moonapp_host *)calloc(1, sizeof(moonapp_host));
  if (host == NULL) return NULL;
  moonapp_queue_init(&host->queue);
  host->next_id = 1;
  host->instance = GetModuleHandleW(NULL);
  set_dpi_aware(host);
  return host;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_host_ok(moonapp_host *host) { return host != NULL ? 1 : 0; }

MOONBIT_FFI_EXPORT
int32_t moonapp_host_error(moonapp_host *host, char *into, int32_t cap) {
  const char *message = host == NULL ? "no host" : host->error;
  if (into == NULL || cap <= 0) return 0;
  int32_t len = (int32_t)strlen(message);
  if (len > cap - 1) len = cap - 1;
  memcpy(into, message, (size_t)len);
  into[len] = 0;
  return len;
}

MOONBIT_FFI_EXPORT
void moonapp_host_free(moonapp_host *host) {
  if (host == NULL) return;
  for (int32_t i = 0; i < host->count; i++) {
    moonapp_win *win = &host->windows[i];
    if (win->alive && win->hwnd != NULL) {
      SetWindowLongPtrW(win->hwnd, GWLP_USERDATA, 0);
      DestroyWindow(win->hwnd);
    }
    release_frame(win);
    if (win->cursor_handle != NULL) DestroyCursor(win->cursor_handle);
  }
  free(host);
}

MOONBIT_FFI_EXPORT
int32_t moonapp_open(moonapp_host *host, const char *title, int32_t title_len,
                     const char *app_id, int32_t app_id_len, int32_t width,
                     int32_t height, int32_t flags) {
  if (host == NULL) return -1;
  if (host->count >= MOONAPP_MAX_WINDOWS) {
    moonapp_error(host->error, "no room for another window");
    return -1;
  }
  WCHAR *wide_title = widen(title, title_len);
  WCHAR *wide_app = widen(app_id, app_id_len);
  if (wide_title == NULL || wide_app == NULL) {
    free(wide_title);
    free(wide_app);
    moonapp_error(host->error, "out of memory for the window name");
    return -1;
  }
  moonapp_win *win = &host->windows[host->count];
  memset(win, 0, sizeof(*win));
  win->host = host;
  win->id = host->next_id++;
  win->alive = 1;
  win->scale = 1.0;
  win->cursor = 0;
  win->ime_allowed = 1;
  WNDCLASSEXW klass;
  memset(&klass, 0, sizeof(klass));
  klass.cbSize = sizeof(klass);
  klass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  klass.lpfnWndProc = window_proc;
  klass.hInstance = host->instance;
  klass.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
  klass.lpszClassName = wide_app;
  if (RegisterClassExW(&klass) == 0 &&
      GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    moonapp_error(host->error, "the desktop refused the window class");
    free(wide_title);
    free(wide_app);
    return -1;
  }
  DWORD style = WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  if (flags & MOONAPP_FLAG_DECORATIONS) {
    style |= WS_OVERLAPPEDWINDOW;
  } else {
    style |= WS_POPUP;
  }
  if (!(flags & MOONAPP_FLAG_RESIZABLE)) {
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }
  win->scale = 1.0;
  RECT rect = {0, 0, width, height};
  if (flags & MOONAPP_FLAG_DECORATIONS) {
    DWORD dpi = 96;
    if (p_get_dpi_for_window != NULL) {
      /* Before there is a window, the screen it will open on decides. */
      HDC screen = GetDC(NULL);
      if (screen != NULL) {
        dpi = (DWORD)GetDeviceCaps(screen, LOGPIXELSX);
        ReleaseDC(NULL, screen);
      }
    }
    if (p_adjust_window_rect_ex_for_dpi != NULL) {
      p_adjust_window_rect_ex_for_dpi(&rect, style, FALSE, WS_EX_APPWINDOW,
                                      (UINT)dpi);
    } else {
      AdjustWindowRectEx(&rect, style, FALSE, WS_EX_APPWINDOW);
    }
  }
  HWND hwnd = CreateWindowExW(
      WS_EX_APPWINDOW, wide_app, wide_title, style, CW_USEDEFAULT, CW_USEDEFAULT,
      rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, host->instance,
      win);
  free(wide_title);
  free(wide_app);
  if (hwnd == NULL) {
    moonapp_error(host->error, "the desktop refused the window");
    win->alive = 0;
    return -1;
  }
  win->hwnd = hwnd;
  win->cursor_handle = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
  host->count++;
  RECT client;
  GetClientRect(hwnd, &client);
  win->width = client.right - client.left;
  win->height = client.bottom - client.top;
  if (p_get_dpi_for_window != NULL) {
    UINT dpi = p_get_dpi_for_window(hwnd);
    if (dpi > 0) win->scale = (double)dpi / 96.0;
  }
  if (flags & MOONAPP_FLAG_VISIBLE) {
    ShowWindow(hwnd, SW_SHOW);
  }
  return win->id;
}

static moonapp_win *find_window(moonapp_host *host, int32_t id) {
  for (int32_t i = 0; i < host->count; i++) {
    if (host->windows[i].alive && host->windows[i].id == id) {
      return &host->windows[i];
    }
  }
  return NULL;
}

MOONBIT_FFI_EXPORT
void moonapp_close(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL) return;
  if (win->hwnd != NULL) DestroyWindow(win->hwnd);
  release_frame(win);
  win->alive = 0;
}

MOONBIT_FFI_EXPORT
double moonapp_primary_scale(moonapp_host *host) {
  (void)host;
  HDC screen = GetDC(NULL);
  if (screen == NULL) return 1.0;
  int32_t dpi = GetDeviceCaps(screen, LOGPIXELSX);
  ReleaseDC(NULL, screen);
  return dpi > 0 ? (double)dpi / 96.0 : 1.0;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_is_shutdown(moonapp_host *host) {
  return host == NULL ? 1 : host->quitting;
}

MOONBIT_FFI_EXPORT
void moonapp_host_shutdown(moonapp_host *host) {
  if (host == NULL) return;
  host->quitting = 1;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_window_open(moonapp_host *host, int32_t id) {
  return find_window(host, id) != NULL;
}

/* Pump the desktop's messages into the queue, and report whether one is ready. */
static int32_t pump(moonapp_host *host, int32_t timeout_ms, int32_t wait) {
  if (host->has_current) return 1;
  if (moonapp_queue_pop(&host->queue, &host->current)) {
    host->has_current = 1;
    return 1;
  }
  if (wait) {
    MsgWaitForMultipleObjectsEx(0, NULL, (DWORD)timeout_ms, QS_ALLINPUT,
                                MWMO_INPUTAVAILABLE);
  }
  MSG message;
  while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
    if (message.message == WM_QUIT) {
      host->quitting = 1;
      continue;
    }
    TranslateMessage(&message);
    DispatchMessageW(&message);
    if (moonapp_queue_pop(&host->queue, &host->current)) {
      host->has_current = 1;
      return 1;
    }
  }
  return 0;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_poll(moonapp_host *host) { return pump(host, 0, 0); }

MOONBIT_FFI_EXPORT
int32_t moonapp_wait(moonapp_host *host, int32_t timeout_ms) {
  return pump(host, timeout_ms, 1);
}

MOONBIT_FFI_EXPORT
void moonapp_event_release(moonapp_host *host) {
  if (host != NULL) host->has_current = 0;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_event_kind(moonapp_host *host) { return host->current.kind; }

MOONBIT_FFI_EXPORT
int32_t moonapp_event_window(moonapp_host *host) { return host->current.window; }

MOONBIT_FFI_EXPORT
int32_t moonapp_event_i32(moonapp_host *host, int32_t which) {
  switch (which) {
    case 0: return host->current.i32a;
    case 1: return host->current.i32b;
    case 2: return host->current.i32c;
    default: return host->current.i32d;
  }
}

MOONBIT_FFI_EXPORT
double moonapp_event_f64(moonapp_host *host, int32_t which) {
  return which == 0 ? host->current.f64a : host->current.f64b;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_event_text_len(moonapp_host *host) {
  return host->current.text_len;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_event_text(moonapp_host *host, uint8_t *buffer, int32_t cap) {
  int32_t len = host->current.text_len;
  if (buffer == NULL || cap < len) return len;
  memcpy(buffer, host->current.text, (size_t)len);
  return len;
}

/* ------------------------------------------------------------------------- */
/* Window properties                                                          */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
void moonapp_set_title(moonapp_host *host, int32_t id, const char *title,
                       int32_t len) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL || win->hwnd == NULL) return;
  WCHAR *wide = widen(title, len);
  if (wide == NULL) return;
  SetWindowTextW(win->hwnd, wide);
  free(wide);
}

MOONBIT_FFI_EXPORT
void moonapp_set_size(moonapp_host *host, int32_t id, int32_t width,
                      int32_t height) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL || win->hwnd == NULL) return;
  SetWindowPos(win->hwnd, NULL, 0, 0, width, height,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

MOONBIT_FFI_EXPORT
void moonapp_set_min_size(moonapp_host *host, int32_t id, int32_t width,
                          int32_t height) {
  (void)host;
  (void)id;
  (void)width;
  (void)height;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_width(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  return win == NULL ? 0 : win->width;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_height(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  return win == NULL ? 0 : win->height;
}

MOONBIT_FFI_EXPORT
double moonapp_scale(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  return win == NULL ? 1.0 : win->scale;
}

MOONBIT_FFI_EXPORT
void moonapp_set_cursor(moonapp_host *host, int32_t id, int32_t cursor) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL) return;
  win->cursor = cursor;
  win->cursor_handle = LoadCursorW(NULL, cursor_name(cursor));
  if (win->cursor_handle == NULL) {
    win->cursor_handle = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
  }
  if (win->hwnd != NULL) {
    SetCursor(win->cursor_handle);
  }
}

/* ------------------------------------------------------------------------- */
/* Frames on screen                                                           */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
int32_t moonapp_present(moonapp_host *host, int32_t id, uint8_t *bgra,
                        int32_t width, int32_t height, int32_t stride) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL || bgra == NULL) return 0;
  if (!ensure_frame(win)) return 0;
  int32_t row = width * 4;
  int32_t copy_width = width < win->width ? width : win->width;
  int32_t copy_height = height < win->height ? height : win->height;
  uint8_t *bits = (uint8_t *)win->dib_bits;
  int32_t target_stride = win->width * 4;
  for (int32_t y = 0; y < copy_height; y++) {
    memcpy(bits + (size_t)y * target_stride, bgra + (size_t)y * stride,
           (size_t)(copy_width * 4));
  }
  HDC dc = GetDC(win->hwnd);
  if (dc != NULL) {
    blit_frame(win, dc);
    ReleaseDC(win->hwnd, dc);
  }
  return 1;
}

MOONBIT_FFI_EXPORT
int32_t moonapp_read_back(moonapp_host *host, int32_t id, uint8_t *out,
                          int32_t cap) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL || win->hwnd == NULL) return -1;
  int32_t width = win->width;
  int32_t height = win->height;
  if (width <= 0 || height <= 0) return -1;
  int32_t need = width * height * 4;
  if (out == NULL || cap < need) return need;
  BITMAPINFO info;
  memset(&info, 0, sizeof(info));
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void *bits = NULL;
  HDC screen = GetDC(NULL);
  HDC memory = CreateCompatibleDC(screen);
  HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL,
                                    0);
  ReleaseDC(NULL, screen);
  if (memory == NULL || bitmap == NULL) {
    if (memory != NULL) DeleteDC(memory);
    if (bitmap != NULL) DeleteObject(bitmap);
    return -1;
  }
  HGDIOBJ previous = SelectObject(memory, bitmap);
  /* PW_CLIENTONLY asks the window to draw its client area into the given
   * context, which is what WM_PRINTCLIENT here does; a window that refuses
   * falls back to copying what is on screen. */
  int32_t drawn = 0;
  if (PrintWindow(win->hwnd, memory, PW_CLIENTONLY)) drawn = 1;
  if (!drawn) {
    HDC dc = GetDC(win->hwnd);
    if (dc != NULL) {
      drawn = BitBlt(memory, 0, 0, width, height, dc, 0, 0, SRCCOPY);
      ReleaseDC(win->hwnd, dc);
    }
  }
  if (drawn) {
    memcpy(out, bits, (size_t)need);
  }
  SelectObject(memory, previous);
  DeleteObject(bitmap);
  DeleteDC(memory);
  return drawn ? need : -1;
}

/* ------------------------------------------------------------------------- */
/* Clipboard                                                                  */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
int32_t moonapp_clipboard_get(moonapp_host *host, uint8_t *buffer, int32_t cap) {
  (void)host;
  if (!OpenClipboard(NULL)) return -1;
  HANDLE handle = GetClipboardData(CF_UNICODETEXT);
  if (handle == NULL) {
    CloseClipboard();
    return -1;
  }
  const WCHAR *wide = (const WCHAR *)GlobalLock(handle);
  if (wide == NULL) {
    CloseClipboard();
    return -1;
  }
  int32_t size = (int32_t)lstrlenW(wide);
  int32_t need = WideCharToMultiByte(CP_UTF8, 0, wide, size, NULL, 0, NULL, NULL);
  if (need < 0) need = 0;
  if (buffer != NULL && cap >= need && need > 0) {
    WideCharToMultiByte(CP_UTF8, 0, wide, size, (char *)buffer, cap, NULL, NULL);
  }
  GlobalUnlock(handle);
  CloseClipboard();
  return need;
}

MOONBIT_FFI_EXPORT
void moonapp_clipboard_set(moonapp_host *host, const char *text, int32_t len) {
  (void)host;
  if (!OpenClipboard(NULL)) return;
  EmptyClipboard();
  WCHAR *wide = widen(text, len);
  if (wide != NULL) {
    int32_t units = (int32_t)lstrlenW(wide);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (size_t)(units + 1) *
                                                    sizeof(WCHAR));
    if (memory != NULL) {
      WCHAR *copy = (WCHAR *)GlobalLock(memory);
      if (copy != NULL) {
        memcpy(copy, wide, (size_t)(units + 1) * sizeof(WCHAR));
        GlobalUnlock(memory);
        SetClipboardData(CF_UNICODETEXT, memory);
      }
    }
    free(wide);
  }
  CloseClipboard();
}

/* ------------------------------------------------------------------------- */
/* Input methods                                                              */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
void moonapp_set_ime_allowed(moonapp_host *host, int32_t id, int32_t allowed) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL) return;
  win->ime_allowed = allowed;
  if (win->hwnd == NULL) return;
  if (p_imm_associate_context_ex != NULL) {
    p_imm_associate_context_ex(win->hwnd, NULL,
                               allowed ? IACE_DEFAULT : 0);
  }
}

MOONBIT_FFI_EXPORT
void moonapp_set_ime_area(moonapp_host *host, int32_t id, int32_t x, int32_t y,
                          int32_t width, int32_t height) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL || win->hwnd == NULL) return;
  if (p_imm_get_context == NULL || p_imm_set_composition_window == NULL) return;
  HIMC context = p_imm_get_context(win->hwnd);
  if (context == NULL) return;
  COMPOSITIONFORM form;
  memset(&form, 0, sizeof(form));
  form.dwStyle = CFS_POINT;
  form.ptCurrentPos.x = x;
  form.ptCurrentPos.y = y;
  p_imm_set_composition_window(context, &form);
  (void)width;
  (void)height;
  if (p_imm_release_context != NULL) p_imm_release_context(win->hwnd, context);
}

/* ------------------------------------------------------------------------- */
/* Native handles, for a GPU surface                                          */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
int32_t moonapp_native_kind(void) { return MOONAPP_PLATFORM_WINDOWS; }

MOONBIT_FFI_EXPORT
uint64_t moonapp_native_display(moonapp_host *host) {
  (void)host;
  return (uint64_t)(uintptr_t)GetModuleHandleW(NULL);
}

MOONBIT_FFI_EXPORT
uint64_t moonapp_native_window(moonapp_host *host, int32_t id) {
  moonapp_win *win = find_window(host, id);
  return win == NULL ? 0 : (uint64_t)(uintptr_t)win->hwnd;
}

/* ------------------------------------------------------------------------- */
/* Test hooks                                                                 */
/*                                                                           */
/* The tests drive a window the way the desktop does: by putting messages in   */
/* its queue. These are not part of the library's interface and no MoonBit     */
/* module outside a test declares them.                                       */
/* ------------------------------------------------------------------------- */

MOONBIT_FFI_EXPORT
int32_t moonapp_test_key(moonapp_host *host, int32_t id, int32_t vk,
                         int32_t down) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL || win->hwnd == NULL) return 0;
  LPARAM lparam = (LPARAM)(MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC) << 16);
  if (!down) lparam |= (LPARAM)(1 << 30) | (LPARAM)(1 << 31);
  return PostMessageW(win->hwnd, down ? WM_KEYDOWN : WM_KEYUP, (WPARAM)vk,
                      lparam);
}

MOONBIT_FFI_EXPORT
int32_t moonapp_test_text(moonapp_host *host, int32_t id, int32_t unit) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL || win->hwnd == NULL) return 0;
  return PostMessageW(win->hwnd, WM_CHAR, (WPARAM)unit, 1);
}

MOONBIT_FFI_EXPORT
int32_t moonapp_test_pointer(moonapp_host *host, int32_t id, int32_t kind,
                             int32_t x, int32_t y, int32_t button) {
  moonapp_win *win = find_window(host, id);
  if (win == NULL || win->hwnd == NULL) return 0;
  LPARAM at = MAKELPARAM(x, y);
  UINT message = WM_MOUSEMOVE;
  WPARAM wparam = 0;
  switch (kind) {
    case 0: message = WM_MOUSEMOVE; break;
    case 1: message = WM_LBUTTONDOWN; wparam = MK_LBUTTON; break;
    case 2: message = WM_LBUTTONUP; break;
    case 3: message = WM_RBUTTONDOWN; wparam = MK_RBUTTON; break;
    case 4: message = WM_RBUTTONUP; break;
    case 5:
      message = WM_MOUSEWHEEL;
      /* The wheel carries its distance in the high word and screen coordinates
       * in the low one; the position part is not read by the backend. */
      wparam = MAKEWPARAM(0, (WORD)(y * 120));
      break;
    default: break;
  }
  (void)button;
  return PostMessageW(win->hwnd, message, wparam, at);
}

#endif /* _WIN32 */
