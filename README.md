# moonapp

Native windows and an event loop for a desktop: open a window, hand it frames of
RGBA pixels, and read back the keys, the pointer, the wheel and the composed text
that come out of it. The system interfaces are called through C stubs — Win32 on
Windows, Cocoa through the Objective-C runtime on macOS, Wayland and X11 on
Linux — so no third-party window library is involved and the same code runs on
all three.

The package draws nothing. A frame is a rectangle of pixels a renderer produced;
putting it on screen and turning desktop input into events is what this library
is for.

## Install

```bash
moon add moonbitstack/moonapp
```

The window is native code, so it lives in `moonbitstack/moonapp/host`. The frames,
keys, events and scale arithmetic are in the root package and build on every
target, including the browser ones, which is what lets a layout be tested
without a desktop.

```moonbit
import {
  "moonbitstack/moonapp" @moonapp,
  "moonbitstack/moonapp/host" @host,
}
```

## A window with something in it

```moonbit native
fn run() -> Unit raise @moonapp.AppError {
  let app = @host.App::new()
  let window = app.open()
  let (width, height) = window.size()
  let frame = @moonapp.Frame::new(width, height)
  frame.fill(@moonapp.Rgba::solid(20, 24, 32))
  frame.fill_rect(40, 40, 160, 90, @moonapp.Rgba::solid(220, 90, 60))
  window.present(frame)
  window.close()
  app.close()
}
```

`App::new` asks the desktop for a connection, `App::open` opens a window of the
size in `@moonapp.window_config` — 800 by 600 logical pixels by default — and
`Window::present` puts a frame on it. A frame that is not the size of the client
area is clipped; ask `Window::size` for the size that fits.

## Reading the desktop

One window, one loop, and every event named the same way on every desktop:

```moonbit native
fn pump(app : @host.App, window : @host.Window) -> Unit {
  while !app.is_shutdown() {
    match app.wait(timeout=16) {
      Some((_, @moonapp.CloseRequested)) => window.close()
      Some((_, @moonapp.KeyDown(key))) => {
        if key.key == @moonapp.Key::Escape {
          window.close()
        }
      }
      Some((_, @moonapp.Text(text))) => println(text)
      Some((_, @moonapp.Resized(width, height))) =>
        println("\{width} by \{height}")
      _ => ()
    }
  }
}
```

`App::wait` blocks for at most the given number of milliseconds and returns the
window the event belongs to with the event itself; `App::poll` looks once and
returns nothing when the queue is empty. Sizes and pointer positions are physical
pixels, the size a frame is drawn in.

A key press and the text it produces are separate events. `KeyDown` says which
key, wherever the layout has put it; `Text` says what to insert, after the layout
and any input method have had their say, so a shortcut binds to `Key` and an
editor inserts `Text`.

## Frames

```moonbit
fn paint() -> @moonapp.Frame {
  let frame = @moonapp.Frame::new(64, 48)
  frame.fill(@moonapp.Rgba::solid(12, 12, 12))
  for y in 0..<48 {
    for x in 0..<64 {
      let red = (x * 4).to_double() / 64.0
      let alpha = (y * 255 / 47)
      frame.set(x, y, @moonapp.Rgba::new((red * 255.0).to_int(), 200, 80, alpha))
    }
  }
  frame
}
```

A `Frame` holds straight alpha, eight bits a channel, first row at the top. Each
backend converts that to the byte order its desktop wants on the way out, and
`Frame::to_host_order` and `Frame::from_host_order` are that conversion: a caller
who writes a GPU read-back into a frame uses the same two functions.

`Window::read_back` reads the window as a frame, which is what the tests compare
against what was drawn. Win32 and X11 ask the desktop for the window's content;
macOS keeps what a window shows to itself, so there the answer is the pixels the
window was given, which are the ones a person sees. Wayland has no such request
in its protocol and answers `AppError::Unavailable` there.

## Screens with more pixels

```moonbit native
fn describe(window : @host.Window) -> String {
  let (width, height) = window.size()
  let scale = window.scale()
  let logical_width = @moonapp.to_logical(width.to_double(), scale).to_int()
  "\{width} by \{height} physical, \{logical_width} by \{height} logical, scale \{scale}"
}
```

A configuration asks for logical pixels; a frame is drawn in physical ones. A
screen at 144 dots per inch reports 1.5, and a window asked for as 800 by 600
gets 1200 by 900 pixels to draw in. `Event::ScaleChanged` arrives when the window
moves to a screen with another factor, and the frame is laid out again at the new
size.

## The desktop behind the window

| | Windows | macOS | Linux |
|:--|:--|:--|:--|
| Window | `CreateWindowExW` | `NSWindow` | `xdg_surface`, X11 window |
| Frame | DIB section and `BitBlt` | `CALayer` contents | `wl_shm`, `XPutImage` |
| Input | window messages | `NSEvent` | `wl_keyboard`, X11 events |
| Composed text | `WM_IME_COMPOSITION` | input method of the text system | XIM |
| Clipboard | `CF_UNICODETEXT` | `NSPasteboard` | `PRIMARY` and `CLIPBOARD` |
| Read back | `PrintWindow` | view capture | `XGetImage` |

The libraries a backend needs are resolved when the host is created, so importing
this package adds no linker flag to a build. On Linux both backends are compiled
in: one binary speaks to either desktop, and picks by `MOONAPP_BACKEND` (`x11` or
`wayland`), then by `WAYLAND_DISPLAY`, then by whichever one answers.

## The interface

| | |
|:--|:--|
| `App::new`, `App::close` | Connect to the desktop, or let go of it. |
| `App::open(window_config~)` | Open a window. |
| `App::poll`, `App::wait(timeout~)` | Take the next event, if there is one. |
| `App::platform`, `App::scale` | Which desktop, and how many pixels a point is worth. |
| `Window::present`, `Window::read_back` | Put a frame on screen, take one off it. |
| `Window::size`, `Window::resize`, `Window::scale` | The client area, in physical pixels and in logical ones. |
| `Window::set_title`, `Window::close` | The title bar, and the end of the window. |
| `Window::clipboard`, `Window::set_clipboard` | Text, in and out. |
| `Window::set_cursor` | The shape of the pointer over the window. |
| `Window::set_ime_allowed`, `Window::set_ime_area` | Whether to compose text, and where the caret is. |
| `Window::surface` | The native objects a GPU backend attaches to. |
| `Frame`, `Rgba`, `Frame::to_host_order` | Pixels, and the byte order a desktop wants. |
| `Key`, `Key::name`, `Modifiers`, `MouseButton`, `Event` | What the desktop reports. |
| `ScaleChanged`, `to_physical`, `to_logical`, `to_pixel` | The arithmetic between the two kinds of pixel. |

`Key::from_windows`, `Key::from_keysym` and `Key::from_cocoa` are the three
mappings from a platform's own key codes, for a backend written outside this
package.

## What it does not do

- One `App` per thread. The desktops have one message queue per thread and this
  library reads it.
- Wayland has no request for a window to read itself back, no way for one client
  to inject input into another, no text input protocol here and no clipboard
  transfer here: text is composed through XIM on X11 and through the window
  messages and the text system on the other two, and the clipboard is a
  round trip through the compositor only on the desktops that offer one.
- A pointer shape on Wayland comes from a protocol of its own that not every
  compositor has, so the pointer keeps whatever shape the compositor gave it.
- A transparent window is drawn per pixel on macOS and Wayland; Win32 keeps its
  window opaque.
- No GPU context: `Window::surface` hands out the native objects and stops there.
