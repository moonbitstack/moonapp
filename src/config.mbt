///|
/// How a window is to be created.
///
/// Sizes are logical pixels: what a layout asks for, before the screen's scale
/// factor turns it into the pixels a frame is drawn in. A host that is asked for
/// a size the platform will not grant — smaller than the minimum, larger than the
/// screen — gets the closest size it can and reports it with
/// [`Event::Resized`].
#derive(Debug)
pub(all) struct WindowConfig {
  /// What the title bar shows.
  title : String
  /// Width in logical pixels.
  width : Int32
  /// Height in logical pixels.
  height : Int32
  /// The narrowest the user may drag the window to, in logical pixels; zero for
  /// no limit.
  min_width : Int32
  /// The shortest the user may drag the window to, in logical pixels; zero for
  /// no limit.
  min_height : Int32
  /// Whether the user may resize it. A window that may not is usually a dialog
  /// or a splash screen.
  resizable : Bool
  /// Whether the platform draws a title bar and a frame around it. Without them
  /// the window is a bare rectangle, and dragging it is up to the application.
  decorations : Bool
  /// Whether pixels with alpha below 255 let what is behind the window through.
  /// Only the parts of the screen the window covers are affected, and only where
  /// the platform composites windows with alpha.
  transparent : Bool
  /// Whether it is on screen as soon as it is created. A window that is not can
  /// be drawn into before it is shown, which avoids a flash of an empty frame.
  visible : Bool
  /// Whether the application draws in physical pixels and handles the scale
  /// factor itself. A host that does gets [`Event::ScaleChanged`] and lays its
  /// content out again; a host that does not gets a window the platform scales,
  /// which is blurry but simpler.
  high_dpi : Bool
  /// The name the desktop knows the window by: a Wayland `app_id`, an X11 class,
  /// a Windows class name. Desktop shells group windows by it and file them
  /// under the launcher entry of the same name.
  app_id : String
}

///|
/// A window of 800 by 600 logical pixels: titled, resizable, decorated, opaque,
/// visible, drawn at the screen's own resolution, and filed under a name no
/// launcher owns.
pub let window_config : WindowConfig = {
  title: "moonapp",
  width: 800,
  height: 600,
  min_width: 0,
  min_height: 0,
  resizable: true,
  decorations: true,
  transparent: false,
  visible: true,
  high_dpi: true,
  app_id: "moonapp",
}

///|
/// A window that may not be resized or decorated, for a splash screen or a
/// preview.
pub let fixed_window_config : WindowConfig = {
  ..window_config,
  resizable: false,
  decorations: false,
}

///|
/// The name of a window inside an application. Handles stay valid until the
/// window is closed, and a closed window's number is never handed out again.
#derive(Debug, Eq, Hash)
pub(all) struct WindowId {
  value : Int32
}

///|
/// What a caller can do wrong when asking a host for something.
#derive(Debug)
pub(all) suberror AppError {
  /// The host cannot work here: no display server, no window manager, a platform
  /// without an implementation.
  Unavailable(String)
  /// The platform refused a request. The message is what the platform said, in
  /// the platform's words.
  Refused(String)
  /// The platform wrote a buffer in a size the caller did not expect: a window
  /// whose pixels do not match the size the host reported.
  Malformed(String)
}

///|
/// The native object a GPU surface attaches to.
///
/// A window is drawn into by the host, but a GPU backend needs the object behind
/// it: a `wl_surface`, an X11 window with its display, an `NSView`, a `HWND`.
/// `first` and `second` hold them in the order the family documents, and a
/// backend that has nothing to hand out reports [`SurfaceKind::None`].
#derive(Debug, Eq)
pub(all) struct Surface {
  kind : SurfaceKind
  /// The display or connection the window belongs to, or zero.
  first : UInt64
  /// The window itself, or zero.
  second : UInt64
}

///|
/// Which family of native objects [`Surface`] carries, so a GPU backend can tell
/// what it was handed.
#derive(Debug, Eq)
pub(all) enum SurfaceKind {
  /// There is no surface to draw on.
  None
  /// Windows: the `HWND`.
  Win32
  /// macOS: the `NSView`.
  Cocoa
  /// X11: the `Display` in `first` and the window in `second`.
  X11
  /// Wayland: the `wl_display` in `first` and the `wl_surface` in `second`.
  Wayland
}
