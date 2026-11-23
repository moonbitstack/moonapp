///|
/// The desktop whose native codes an event carries. A host that writes its own
/// backend needs it only to pick the right key mapper; a consumer never does.
#derive(Debug, Eq)
pub(all) enum Platform {
  Windows
  Macos
  X11
  Wayland
  /// A platform this library has no backend for.
  Other
}

///|
/// The desktop belonging to a number a host reported.
pub fn Platform::from_code(code : Int32) -> Platform {
  match code {
    0 => Windows
    1 => Macos
    2 => X11
    3 => Wayland
    _ => Other
  }
}

///|
/// The key a platform's own code names, in whichever vocabulary that platform
/// speaks: a virtual-key code on Windows, a keysym on X11 and Wayland, a virtual
/// key code on macOS. A code for a key this library does not name becomes
/// [`Key::Unknown`] carrying the code, so an application can still tell one
/// apart from another.
pub fn Key::from_platform(platform : Platform, code : Int32) -> Key {
  match platform {
    Windows => Key::from_windows(code)
    Macos => Key::from_cocoa(code)
    X11 | Wayland => Key::from_keysym(code)
    Other => Unknown(code)
  }
}
