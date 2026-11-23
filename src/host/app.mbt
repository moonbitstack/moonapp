///|
/// A window, and the desktop connection behind it.
///
/// One `App` per process in practice: a desktop has one message queue per thread,
/// and a second `App` on the same thread would take the first one's messages. Get
/// one with [`App::new`], open windows from it, poll it until the application is
/// done, and release it with [`App::close`].
///
/// The sizes in the interface come in two kinds. Logical pixels are what a layout
/// asks for and what the user thinks in; physical pixels are what a frame is
/// drawn in and what [`Event::Resized`] reports. [`Window::scale`] is the number
/// between them.
pub struct App {
  host : Host
  platform : @moonapp.Platform
}

///|
/// A window of an [`App`].
///
/// A handle is small and may be copied: both copies refer to the same window. A
/// closed window's handle answers zeroes and empty strings rather than failing.
pub struct Window {
  app : App
  id : @moonapp.WindowId
}

///|
fn host_error_text(host : Host) -> String {
  let buffer = FixedArray::make(512, b'\x00')
  let len = host_error(host, buffer, 512)
  if len <= 0 {
    ""
  } else {
    @utf8.decode_lossy(Bytes::from_array(buffer[:]))
  }
}

///|
/// Ask the desktop for a connection.
///
/// # Errors
///
/// [`AppError::Unavailable`] when the desktop cannot be reached: no display
/// server, no session to open a window in, or a platform this library has no
/// backend for.
pub fn App::new() -> App raise @moonapp.AppError {
  let host = host_new()
  if host_ok(host) == 0 {
    raise Unavailable("the desktop refused a connection")
  }
  let platform = @moonapp.Platform::from_code(native_kind())
  { host, platform, }
}

///|
/// The desktop this connection speaks to.
pub fn App::platform(self : App) -> @moonapp.Platform {
  self.platform
}

///|
/// How many physical pixels one logical pixel is worth before any window exists,
/// which is what the size of a new window is scaled by.
pub fn App::scale(self : App) -> Double {
  primary_scale(self.host)
}

///|
/// Open a window.
///
/// The size in the configuration is logical pixels and is turned into the
/// screen's own pixels here. A window whose size the desktop will not grant
/// reports what it did grant with [`Event::Resized`].
///
/// # Errors
///
/// [`AppError::Refused`] when the desktop refuses the window, with the reason it
/// gave.
pub fn App::open(
  self : App,
  window_config~ : @moonapp.WindowConfig = @moonapp.window_config,
) -> Window raise @moonapp.AppError {
  let scale = self.scale()
  let logical_width = window_config.width.to_double()
  let logical_height = window_config.height.to_double()
  let title = @utf8.encode(window_config.title)
  let app_id = @utf8.encode(window_config.app_id)
  let id = open_window(
    self.host,
    title,
    title.length(),
    app_id,
    app_id.length(),
    @moonapp.to_extent(@moonapp.to_physical(logical_width, scale)),
    @moonapp.to_extent(@moonapp.to_physical(logical_height, scale)),
    open_flags(window_config),
  )
  if id < 0 {
    let text = host_error_text(self.host)
    raise Refused(
      if text == "" {
        "the desktop refused the window"
      } else {
        text
      },
    )
  }
  let window : Window = { app: self, id: @moonapp.WindowId::{ value: id, }, }
  if window_config.min_width > 0 || window_config.min_height > 0 {
    let scale = window.scale()
    let width = if window_config.min_width > 0 {
      @moonapp.to_extent(
        @moonapp.to_physical(window_config.min_width.to_double(), scale),
      )
    } else {
      0
    }
    let height = if window_config.min_height > 0 {
      @moonapp.to_extent(
        @moonapp.to_physical(window_config.min_height.to_double(), scale),
      )
    } else {
      0
    }
    set_min_size(self.host, id, width, height)
  }
  window
}

///|
/// The parts of a configuration the desktop needs when the window is created,
/// packed into one number rather than a dozen arguments.
fn open_flags(config : @moonapp.WindowConfig) -> Int32 {
  let mut flags = 0
  if config.decorations {
    flags = flags | 1
  }
  if config.resizable {
    flags = flags | 2
  }
  if config.transparent {
    flags = flags | 4
  }
  if config.visible {
    flags = flags | 8
  }
  if config.high_dpi {
    flags = flags | 16
  }
  flags
}

///|
/// Close every window and let the desktop connection go. An application that
/// stops without calling this leaves the connection to the process exit.
pub fn App::close(self : App) -> Unit {
  host_free(self.host)
}

///|
/// Stop the event loop at its next turn, which is how an application that ends
/// without closing its last window leaves.
pub fn App::shutdown(self : App) -> Unit {
  shutdown(self.host)
}

///|
/// Whether the application asked to stop.
pub fn App::is_shutdown(self : App) -> Bool {
  is_shutdown(self.host) != 0
}

///|
/// The next event, if one is waiting.
pub fn App::poll(self : App) -> (@moonapp.WindowId, @moonapp.Event)? {
  if poll(self.host) == 0 {
    return None
  }
  Some(self.take_event())
}

///|
/// The next event, waiting at most `timeout` milliseconds. A negative timeout
/// waits until there is one.
pub fn App::wait(
  self : App,
  timeout~ : Int32 = -1,
) -> (@moonapp.WindowId, @moonapp.Event)? {
  if wait(self.host, timeout) == 0 {
    return None
  }
  Some(self.take_event())
}

///|
fn App::take_event(self : App) -> (@moonapp.WindowId, @moonapp.Event) {
  let id = @moonapp.WindowId::{ value: event_window(self.host), }
  let event = self.decode()
  event_release(self.host)
  (id, event)
}

///|
/// Turn the event the backend is holding into an [`Event`].
fn App::decode(self : App) -> @moonapp.Event {
  let host = self.host
  match event_kind(host) {
    0 => @moonapp.Resized(event_i32(host, 0), event_i32(host, 1))
    1 => @moonapp.ScaleChanged(event_f64(host, 0))
    2 => @moonapp.Focused(event_i32(host, 0) != 0)
    3 => @moonapp.CloseRequested
    4 => @moonapp.KeyDown(self.key_event())
    5 => @moonapp.KeyUp(self.key_event())
    6 => @moonapp.Text(event_string(host))
    7 =>
      @moonapp.Ime(
        @moonapp.Preedit(
          event_string(host),
          cursor=event_i32(host, 0),
          anchor=event_i32(host, 1),
        ),
      )
    8 => @moonapp.Ime(@moonapp.Commit(event_string(host)))
    9 => @moonapp.Ime(@moonapp.Enabled)
    10 => @moonapp.Ime(@moonapp.Disabled)
    11 => @moonapp.PointerMoved(event_f64(host, 0), event_f64(host, 1))
    12 => @moonapp.PointerEntered
    13 => @moonapp.PointerLeft
    14 => @moonapp.ButtonDown(mouse_button(event_i32(host, 0)))
    15 => @moonapp.ButtonUp(mouse_button(event_i32(host, 0)))
    16 =>
      @moonapp.Wheel({
        x: event_f64(host, 0),
        y: event_f64(host, 1),
        unit: if event_i32(host, 0) == 0 {
          @moonapp.Line
        } else {
          @moonapp.Pixel
        },
      })
    17 =>
      @moonapp.Gesture(
        if event_i32(host, 0) == 0 {
          @moonapp.Magnify(event_f64(host, 0))
        } else {
          @moonapp.Rotate(event_f64(host, 0))
        },
      )
    // A backend that reports something this package cannot name reports a
    // resize, which a host already has to handle, instead of dropping it.
    _ => @moonapp.Resized(event_i32(host, 0), event_i32(host, 1))
  }
}

///|
fn App::key_event(self : App) -> @moonapp.KeyEvent {
  let host = self.host
  {
    key: @moonapp.Key::from_platform(self.platform, event_i32(host, 0)),
    repeat: event_i32(host, 2) != 0,
    modifiers: @moonapp.Modifiers::from_bits(event_i32(host, 1)),
  }
}

///|
fn mouse_button(code : Int32) -> @moonapp.MouseButton {
  match code {
    0 => Left
    1 => Right
    2 => Middle
    3 => Back
    4 => Forward
    other => Other(other)
  }
}

///|
/// The text the backend is holding, as UTF-8.
fn event_string(host : Host) -> String {
  let len = event_text_len(host)
  if len <= 0 {
    return ""
  }
  let buffer = FixedArray::make(len, b'\x00')
  let _ = event_text(host, buffer, len)
  @utf8.decode_lossy(Bytes::from_array(buffer[:]))
}

///|
/// The number of this window, which is what the event loop reports with it.
pub fn Window::id(self : Window) -> @moonapp.WindowId {
  self.id
}

///|
/// Whether this window is still on the desktop.
pub fn Window::is_open(self : Window) -> Bool {
  window_is_open(self.app.host, self.id.value) != 0
}

///|
/// Close the window.
pub fn Window::close(self : Window) -> Unit {
  close_window(self.app.host, self.id.value)
}

///|
/// Change what the title bar says.
pub fn Window::set_title(self : Window, title : String) -> Unit {
  let bytes = @utf8.encode(title)
  set_title(self.app.host, self.id.value, bytes, bytes.length())
}

///|
/// Ask for a client area of `width` by `height` logical pixels.
pub fn Window::resize(self : Window, width : Int32, height : Int32) -> Unit {
  let scale = self.scale()
  set_size(
    self.app.host,
    self.id.value,
    @moonapp.to_extent(@moonapp.to_physical(width.to_double(), scale)),
    @moonapp.to_extent(@moonapp.to_physical(height.to_double(), scale)),
  )
}

///|
/// The client area in physical pixels, which is the size a frame has to be.
pub fn Window::size(self : Window) -> (Int32, Int32) {
  (
    width_of(self.app.host, self.id.value),
    height_of(self.app.host, self.id.value),
  )
}

///|
/// How many physical pixels one logical pixel is worth on the screen this window
/// is on: 1.0 on a classic screen, 2.0 on a high-density one.
pub fn Window::scale(self : Window) -> Double {
  scale_of(self.app.host, self.id.value)
}

///|
/// Put a frame on screen. A frame of another size is clipped to what fits.
pub fn Window::present(self : Window, frame : @moonapp.Frame) -> Unit {
  if frame.is_empty() {
    return
  }
  let bgra = frame.to_host_order()
  let _ = present(
    self.app.host,
    self.id.value,
    bgra,
    frame.width,
    frame.height,
    frame.width * 4,
  )
}

///|
/// Read what the window shows back as a frame, so a test can compare what was
/// drawn with what the desktop holds.
///
/// Not every desktop lets a window read itself back: Wayland has no such request
/// and the backend there answers [`AppError::Unavailable`].
///
/// # Errors
///
/// [`AppError::Refused`] when the desktop would not give the pixels, and
/// [`AppError::Malformed`] when it gave fewer than the window's size.
pub fn Window::read_back(
  self : Window,
) -> @moonapp.Frame raise @moonapp.AppError {
  let host = self.app.host
  let id = self.id.value
  let need = read_back(host, id, FixedArray::make(0, b'\x00'), 0)
  if need <= 0 {
    raise Unavailable("this desktop does not let a window read itself back")
  }
  let buffer = FixedArray::make(need, b'\x00')
  if read_back(host, id, buffer, need) != need {
    raise Malformed("the read-back buffer is not the size of the window")
  }
  let (width, height) = self.size()
  @moonapp.Frame::from_host_order(width, height, width * 4, buffer)
}

///|
/// What is on the desktop's clipboard, as text. A clipboard holding something
/// that is not text reads as an empty string.
///
/// # Errors
///
/// [`AppError::Refused`] when the desktop would not hand its clipboard over,
/// which happens while another application holds it.
pub fn Window::clipboard(self : Window) -> String raise @moonapp.AppError {
  let host = self.app.host
  let need = clipboard_get(host, FixedArray::make(0, b'\x00'), 0)
  if need <= 0 {
    return ""
  }
  let buffer = FixedArray::make(need, b'\x00')
  let got = clipboard_get(host, buffer, need)
  if got < 0 {
    raise Refused("the clipboard would not give its text")
  }
  @utf8.decode_lossy(Bytes::from_array(buffer[:got]))
}

///|
/// Put text on the desktop's clipboard, replacing what was there.
pub fn Window::set_clipboard(self : Window, text : String) -> Unit {
  let bytes = @utf8.encode(text)
  clipboard_set(self.app.host, bytes, bytes.length())
}

///|
/// Tell the desktop whether this window accepts composed text. A window that does
/// not is one where every key is a command, like a game.
pub fn Window::set_ime_allowed(self : Window, allowed : Bool) -> Unit {
  set_ime_allowed(self.app.host, self.id.value, if allowed { 1 } else { 0 })
}

///|
/// Where the caret is, in physical pixels, so the candidate window of an input
/// method opens next to the text being composed rather than in a corner.
pub fn Window::set_ime_area(
  self : Window,
  x : Int32,
  y : Int32,
  width : Int32,
  height : Int32,
) -> Unit {
  set_ime_area(self.app.host, self.id.value, x, y, width, height)
}

///|
/// What the pointer looks like over this window.
pub fn Window::set_cursor(self : Window, cursor : @moonapp.Cursor) -> Unit {
  set_cursor(self.app.host, self.id.value, cursor_code(cursor))
}

///|
fn cursor_code(cursor : @moonapp.Cursor) -> Int32 {
  match cursor {
    Default => 0
    Pointer => 1
    Text => 2
    Crosshair => 3
    Move => 4
    NotAllowed => 5
    Wait => 6
    Progress => 7
    Help => 8
    ResizeNS => 9
    ResizeEW => 10
    ResizeNESW => 11
    ResizeNWSE => 12
    Grab => 13
    Grabbing => 14
    Zoom => 15
    Cell => 16
    AllScroll => 17
    ColResize => 18
    RowResize => 19
    Hidden => 20
  }
}

///|
/// The native objects behind the window, for a GPU surface to attach to.
pub fn Window::surface(self : Window) -> @moonapp.Surface {
  let host = self.app.host
  let kind = match self.app.platform {
    @moonapp.Windows => @moonapp.Win32
    @moonapp.Macos => @moonapp.Cocoa
    @moonapp.X11 => @moonapp.X11
    @moonapp.Wayland => @moonapp.Wayland
    @moonapp.Other => @moonapp.None
  }
  {
    kind,
    first: native_display(host),
    second: native_window(host, self.id.value),
  }
}
