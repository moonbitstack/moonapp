///|
test "windows virtual keys land on the key the cap says" {
  assert_eq(Key::from_windows(0x41), Key::A)
  assert_eq(Key::from_windows(0x5A), Key::Z)
  assert_eq(Key::from_windows(0x30), Key::Digit0)
  assert_eq(Key::from_windows(0x39), Key::Digit9)
  assert_eq(Key::from_windows(0x0D), Key::Enter)
  assert_eq(Key::from_windows(0x1B), Key::Escape)
  assert_eq(Key::from_windows(0x20), Key::Space)
  assert_eq(Key::from_windows(0x08), Key::Backspace)
  assert_eq(Key::from_windows(0x2E), Key::Delete)
  assert_eq(Key::from_windows(0x25), Key::Left)
  assert_eq(Key::from_windows(0x28), Key::Down)
}

///|
test "windows punctuation is where the OEM codes put it" {
  assert_eq(Key::from_windows(0xBA), Key::Semicolon)
  assert_eq(Key::from_windows(0xBB), Key::Equal)
  assert_eq(Key::from_windows(0xBC), Key::Comma)
  assert_eq(Key::from_windows(0xBD), Key::Minus)
  assert_eq(Key::from_windows(0xBE), Key::Period)
  assert_eq(Key::from_windows(0xBF), Key::Slash)
  assert_eq(Key::from_windows(0xC0), Key::Backquote)
  assert_eq(Key::from_windows(0xDB), Key::BracketLeft)
  assert_eq(Key::from_windows(0xDC), Key::Backslash)
  assert_eq(Key::from_windows(0xDD), Key::BracketRight)
  assert_eq(Key::from_windows(0xDE), Key::Quote)
}

///|
test "windows tells the sides of a modifier apart" {
  assert_eq(Key::from_windows(0xA0), Key::ShiftLeft)
  assert_eq(Key::from_windows(0xA1), Key::ShiftRight)
  assert_eq(Key::from_windows(0xA2), Key::ControlLeft)
  assert_eq(Key::from_windows(0xA3), Key::ControlRight)
  assert_eq(Key::from_windows(0xA4), Key::AltLeft)
  assert_eq(Key::from_windows(0xA5), Key::AltRight)
  assert_eq(Key::from_windows(0x5B), Key::MetaLeft)
  assert_eq(Key::from_windows(0x5C), Key::MetaRight)
  // The unsided codes are the left key.
  assert_eq(Key::from_windows(0x10), Key::ShiftLeft)
  assert_eq(Key::from_windows(0x11), Key::ControlLeft)
  assert_eq(Key::from_windows(0x12), Key::AltLeft)
}

///|
test "windows function keys are one run of codes, then one of keypad keys" {
  assert_eq(Key::from_windows(0x70), Key::F1)
  assert_eq(Key::from_windows(0x7B), Key::F12)
  assert_eq(Key::from_windows(0x87), Key::F24)
  assert_eq(Key::from_windows(0x60), Key::Numpad0)
  assert_eq(Key::from_windows(0x69), Key::Numpad9)
  assert_eq(Key::from_windows(0x6B), Key::NumpadAdd)
  assert_eq(Key::from_windows(0x6F), Key::NumpadDivide)
}

///|
test "a windows code with no name keeps its number" {
  assert_eq(Key::from_windows(0x07), Key::Unknown(0x07))
}

///|
test "keysyms name the key a shifted character was typed on" {
  assert_eq(Key::from_keysym(0x61), Key::A)
  assert_eq(Key::from_keysym(0x41), Key::A)
  assert_eq(Key::from_keysym(0x21), Key::Digit1)
  assert_eq(Key::from_keysym(0x28), Key::Digit9)
  assert_eq(Key::from_keysym(0x40), Key::Digit2)
  assert_eq(Key::from_keysym(0x5F), Key::Minus)
  assert_eq(Key::from_keysym(0x7B), Key::BracketLeft)
  assert_eq(Key::from_keysym(0x3F), Key::Slash)
}

///|
test "keysyms for the edges of the keyboard" {
  assert_eq(Key::from_keysym(0xFF0D), Key::Enter)
  assert_eq(Key::from_keysym(0xFF1B), Key::Escape)
  assert_eq(Key::from_keysym(0xFF51), Key::Left)
  assert_eq(Key::from_keysym(0xFF9C), Key::Unknown(0xFF9C))
  assert_eq(Key::from_keysym(0xFFBE), Key::F1)
  assert_eq(Key::from_keysym(0xFFD5), Key::F24)
  assert_eq(Key::from_keysym(0xFFB5), Key::Numpad5)
  assert_eq(Key::from_keysym(0xFFFF), Key::Delete)
  assert_eq(Key::from_keysym(0xFFE3), Key::ControlLeft)
  assert_eq(Key::from_keysym(0xFFE4), Key::ControlRight)
}

///|
test "cocoa key codes follow the ANSI keyboard, not the alphabet" {
  assert_eq(Key::from_cocoa(0), Key::A)
  assert_eq(Key::from_cocoa(1), Key::S)
  assert_eq(Key::from_cocoa(2), Key::D)
  assert_eq(Key::from_cocoa(6), Key::Z)
  assert_eq(Key::from_cocoa(49), Key::Space)
  assert_eq(Key::from_cocoa(36), Key::Enter)
  assert_eq(Key::from_cocoa(53), Key::Escape)
  assert_eq(Key::from_cocoa(51), Key::Backspace)
  assert_eq(Key::from_cocoa(50), Key::Backquote)
  assert_eq(Key::from_cocoa(123), Key::Left)
  assert_eq(Key::from_cocoa(126), Key::Up)
}

///|
test "cocoa puts command on the left of the pair and control on both sides" {
  assert_eq(Key::from_cocoa(55), Key::MetaLeft)
  assert_eq(Key::from_cocoa(54), Key::MetaRight)
  assert_eq(Key::from_cocoa(59), Key::ControlLeft)
  assert_eq(Key::from_cocoa(62), Key::ControlRight)
  assert_eq(Key::from_cocoa(56), Key::ShiftLeft)
  assert_eq(Key::from_cocoa(60), Key::ShiftRight)
  assert_eq(Key::from_cocoa(63), Key::Fn)
}

///|
test "a platform picks the mapper it speaks" {
  assert_eq(Key::from_platform(Platform::Windows, 0x41), Key::A)
  assert_eq(Key::from_platform(Platform::Macos, 0x00), Key::A)
  assert_eq(Key::from_platform(Platform::X11, 0x61), Key::A)
  assert_eq(Key::from_platform(Platform::Wayland, 0xFF0D), Key::Enter)
  assert_eq(Key::from_platform(Platform::Other, 0x41), Key::Unknown(0x41))
  assert_eq(Platform::from_code(0), Platform::Windows)
  assert_eq(Platform::from_code(3), Platform::Wayland)
  assert_eq(Platform::from_code(9), Platform::Other)
}

///|
test "modifier bits survive a round trip" {
  let held : Modifiers = {
    shift: true,
    control: false,
    alt: true,
    meta: false,
    caps_lock: true,
    num_lock: false,
  }
  assert_eq(held.to_bits(), 21)
  assert_eq(Modifiers::from_bits(21), held)
  assert_eq(Modifiers::from_bits(0).is_empty(), true)
  assert_eq(Modifiers::from_bits(63).is_empty(), false)
}
