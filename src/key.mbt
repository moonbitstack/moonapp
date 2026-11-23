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

///|
/// The name a key is written with, for a shortcut editor or a log: the letter or
/// digit it sits on, the word for a named key, and nothing for one this package
/// has no name for.
pub fn Key::name(self : Key) -> String {
  match self {
    Unknown(_) => ""
    A => "A"
    B => "B"
    C => "C"
    D => "D"
    E => "E"
    F => "F"
    G => "G"
    H => "H"
    I => "I"
    J => "J"
    K => "K"
    L => "L"
    M => "M"
    N => "N"
    O => "O"
    P => "P"
    Q => "Q"
    R => "R"
    S => "S"
    T => "T"
    U => "U"
    V => "V"
    W => "W"
    X => "X"
    Y => "Y"
    Z => "Z"
    Digit0 => "0"
    Digit1 => "1"
    Digit2 => "2"
    Digit3 => "3"
    Digit4 => "4"
    Digit5 => "5"
    Digit6 => "6"
    Digit7 => "7"
    Digit8 => "8"
    Digit9 => "9"
    Minus => "-"
    Equal => "="
    BracketLeft => "["
    BracketRight => "]"
    Backslash => "\\"
    Semicolon => ";"
    Quote => "'"
    Backquote => "`"
    Comma => ","
    Period => "."
    Slash => "/"
    Escape => "Escape"
    Enter => "Enter"
    Tab => "Tab"
    Space => "Space"
    Backspace => "Backspace"
    Delete => "Delete"
    Insert => "Insert"
    Home => "Home"
    End => "End"
    PageUp => "PageUp"
    PageDown => "PageDown"
    Left => "Left"
    Right => "Right"
    Up => "Up"
    Down => "Down"
    ShiftLeft => "Shift"
    ShiftRight => "RightShift"
    ControlLeft => "Control"
    ControlRight => "RightControl"
    AltLeft => "Alt"
    AltRight => "RightAlt"
    MetaLeft => "Meta"
    MetaRight => "RightMeta"
    CapsLock => "CapsLock"
    NumLock => "NumLock"
    ScrollLock => "ScrollLock"
    Fn => "Fn"
    F1 => "F1"
    F2 => "F2"
    F3 => "F3"
    F4 => "F4"
    F5 => "F5"
    F6 => "F6"
    F7 => "F7"
    F8 => "F8"
    F9 => "F9"
    F10 => "F10"
    F11 => "F11"
    F12 => "F12"
    F13 => "F13"
    F14 => "F14"
    F15 => "F15"
    F16 => "F16"
    F17 => "F17"
    F18 => "F18"
    F19 => "F19"
    F20 => "F20"
    F21 => "F21"
    F22 => "F22"
    F23 => "F23"
    F24 => "F24"
    Numpad0 => "Numpad0"
    Numpad1 => "Numpad1"
    Numpad2 => "Numpad2"
    Numpad3 => "Numpad3"
    Numpad4 => "Numpad4"
    Numpad5 => "Numpad5"
    Numpad6 => "Numpad6"
    Numpad7 => "Numpad7"
    Numpad8 => "Numpad8"
    Numpad9 => "Numpad9"
    NumpadAdd => "Numpad+"
    NumpadSubtract => "Numpad-"
    NumpadMultiply => "Numpad*"
    NumpadDivide => "Numpad/"
    NumpadDecimal => "Numpad."
    NumpadEnter => "NumpadEnter"
    PrintScreen => "PrintScreen"
    Pause => "Pause"
    ContextMenu => "ContextMenu"
  }
}
