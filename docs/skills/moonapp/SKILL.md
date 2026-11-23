---
name: moonapp
description: "Native windows and an event loop for desktop applications: open a window, put RGBA frames on it, and read keys, pointer, wheel, cursor and composed text back. Use when an application needs its own window on Windows, macOS or Linux instead of a terminal or a browser."
---

# moonapp

A window, an event loop and a frame buffer, on Win32, Cocoa, Wayland and X11.
The package draws nothing: a renderer produces a frame of RGBA pixels and this
library puts it on screen and turns desktop input into events.

## Install

```bash
moon add moonbitstack/moonapp
```

```moonbit
import {
  "moonbitstack/moonapp" @moonapp,
  "moonbitstack/moonapp/host" @host,
}
```

The window is native code, so it lives in `moonbitstack/moonapp/host`; the frame,
key and event types are in the root package and build on every target.

## A window with a frame in it

```moonbit native
fn show() -> Unit raise @moonapp.AppError {
  let app = @host.App::new()
  let config : @moonapp.WindowConfig = {
    ..@moonapp.window_config,
    title: "hello",
    width: 400,
    height: 300,
  }
  let window = app.open(window_config=config)
  let (width, height) = window.size()
  let frame = @moonapp.Frame::new(width, height)
  frame.fill(@moonapp.Rgba::solid(20, 24, 32))
  window.present(frame)
  window.close()
  app.close()
}
```

## Reading input

```moonbit native
fn pump(app : @host.App, window : @host.Window) -> String raise @moonapp.AppError {
  let mut typed = ""
  match app.wait(timeout=16) {
    Some((_, @moonapp.CloseRequested)) => window.close()
    Some((_, @moonapp.Text(text))) => typed = typed + text
    Some((_, @moonapp.KeyDown(key))) =>
      if key.key == @moonapp.Key::Escape {
        window.close()
      }
    Some((_, @moonapp.Resized(width, height))) =>
      println("\{width} by \{height}")
    _ => ()
  }
  typed
}
```

## Rules that catch people out

- One `App` per thread: a desktop has one message queue per thread and this
  library reads it.
- Sizes and pointer positions in events are **physical** pixels; a
  `WindowConfig` and `Window::resize` are **logical**. `Window::scale` is the
  number between them, and `Event::ScaleChanged` arrives when it changes.
- A key press and the text it types are separate events: bind shortcuts to
  `Key`, insert `Event::Text` into the document.
- A frame must be the size of the client area (`Window::size`) or it is clipped.
  `Frame` holds straight alpha, top row first; the backend converts it.
- `Window::read_back` reads the window as a frame. Win32 and X11 ask the desktop
  for the window's content, macOS hands back the pixels the window was given,
  and on Wayland, where the protocol has no such request, it raises
  `AppError::Unavailable`.
- Call `App::close` when done. `Window::close` alone leaves the connection open.
