///|
/// The physical key that produced an event, named after its position on a US
/// ANSI layout and independent of what the current layout prints on it. Text
/// arrives separately as [`Event::Text`], so typing follows the layout while
/// shortcuts can be bound to a key.
///
/// Every platform names the same key differently: a virtual-key code on
/// Windows, a keysym on X11 and Wayland, a virtual key code on macOS. The
/// three mappers in this package (`Key::from_windows`, `Key::from_keysym`,
/// `Key::from_cocoa`) turn each into one of these, and a backend author can
/// use them without depending on a host.
#derive(Debug, Eq)
pub(all) enum Key {
  Unknown(Int32)
  // Letters, in alphabet order.
  A
  B
  C
  D
  E
  F
  G
  H
  I
  J
  K
  L
  M
  N
  O
  P
  Q
  R
  S
  T
  U
  V
  W
  X
  Y
  Z
  // The number row, then the punctuation beside it.
  Digit0
  Digit1
  Digit2
  Digit3
  Digit4
  Digit5
  Digit6
  Digit7
  Digit8
  Digit9
  Minus
  Equal
  BracketLeft
  BracketRight
  Backslash
  Semicolon
  Quote
  Backquote
  Comma
  Period
  Slash
  // Editing and navigation.
  Escape
  Enter
  Tab
  Space
  Backspace
  Delete
  Insert
  Home
  End
  PageUp
  PageDown
  Left
  Right
  Up
  Down
  // Modifiers, sided because a layout may bind them differently.
  ShiftLeft
  ShiftRight
  ControlLeft
  ControlRight
  AltLeft
  AltRight
  MetaLeft
  MetaRight
  CapsLock
  NumLock
  ScrollLock
  Fn
  // Function keys.
  F1
  F2
  F3
  F4
  F5
  F6
  F7
  F8
  F9
  F10
  F11
  F12
  F13
  F14
  F15
  F16
  F17
  F18
  F19
  F20
  F21
  F22
  F23
  F24
  // The numeric keypad, whose keys are not the number row.
  Numpad0
  Numpad1
  Numpad2
  Numpad3
  Numpad4
  Numpad5
  Numpad6
  Numpad7
  Numpad8
  Numpad9
  NumpadAdd
  NumpadSubtract
  NumpadMultiply
  NumpadDivide
  NumpadDecimal
  NumpadEnter
  // Everything else a desktop has.
  PrintScreen
  Pause
  ContextMenu
}

///|
/// Which of the two keys of a pair was pressed, for backends that report a
/// modifier without saying which side.
#derive(Debug, Eq)
pub(all) enum Side {
  Left
  Right
}

///|
/// The modifier keys held while an event happened.
///
/// `meta` is the key the platform puts next to the space bar and uses for its
/// own shortcuts: Windows on Windows and Linux, Command on macOS.
#derive(Debug, Eq)
pub(all) struct Modifiers {
  shift : Bool
  control : Bool
  alt : Bool
  meta : Bool
  caps_lock : Bool
  num_lock : Bool
}

///|
/// Whether anything that changes the meaning of a key is held.
pub fn Modifiers::is_empty(self : Modifiers) -> Bool {
  !self.shift &&
  !self.control &&
  !self.alt &&
  !self.meta &&
  !self.caps_lock &&
  !self.num_lock
}

///|
/// A mouse button, numbered the way the platforms number them and named the
/// way a desktop names them.
#derive(Debug, Eq)
pub(all) enum MouseButton {
  Left
  Right
  Middle
  Back
  Forward
  Other(Int32)
}

///|
/// One press or release of a key, as the host saw it.
#derive(Debug, Eq)
pub(all) struct KeyEvent {
  key : Key
  /// The autorepeat of a key that is being held down, not a fresh press.
  repeat : Bool
  modifiers : Modifiers
}

///|
/// What the pointer is over, and what a click will do there.
#derive(Debug, Eq)
pub(all) enum Cursor {
  Default
  Pointer
  Text
  Crosshair
  Move
  NotAllowed
  Wait
  Progress
  Help
  ResizeNS
  ResizeEW
  ResizeNESW
  ResizeNWSE
  Grab
  Grabbing
  Zoom
  Cell
  AllScroll
  ColResize
  RowResize
  Hidden
}

///|
/// The modifiers a host reported as a bit set: shift, control, alt, meta, caps
/// lock and num lock in that order, each bit one of them.
pub fn Modifiers::from_bits(bits : Int32) -> Modifiers {
  {
    shift: (bits & 1) != 0,
    control: (bits & 2) != 0,
    alt: (bits & 4) != 0,
    meta: (bits & 8) != 0,
    caps_lock: (bits & 16) != 0,
    num_lock: (bits & 32) != 0,
  }
}

///|
/// The same set of modifiers as the bits [`Modifiers::from_bits`] reads.
pub fn Modifiers::to_bits(self : Modifiers) -> Int32 {
  let mut bits = 0
  if self.shift {
    bits = bits | 1
  }
  if self.control {
    bits = bits | 2
  }
  if self.alt {
    bits = bits | 4
  }
  if self.meta {
    bits = bits | 8
  }
  if self.caps_lock {
    bits = bits | 16
  }
  if self.num_lock {
    bits = bits | 32
  }
  bits
}
