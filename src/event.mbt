///|
/// How a wheel reports its distance. A line is one notch of a classic wheel and
/// is turned into pixels by the consumer, which is the only place that knows the
/// size of the content being scrolled.
#derive(Debug, Eq)
pub(all) enum WheelUnit {
  Line
  Pixel
}

///|
/// One turn of a wheel or one flick of a trackpad, in both axes.
#derive(Debug)
pub(all) struct WheelDelta {
  x : Double
  y : Double
  unit : WheelUnit
}

///|
/// A two-finger gesture on a trackpad.
#derive(Debug)
pub(all) enum Gesture {
  /// Spreading or pinching the fingers; the value is the magnification, so a
  /// pinch reports a factor below one.
  Magnify(Double)
  /// Twisting the fingers; the value is in degrees, positive clockwise.
  Rotate(Double)
}

///|
/// The state of text composition through an input method, for languages whose
/// typing goes through a candidate window.
#derive(Debug)
pub(all) enum Ime {
  /// The host accepted a request to compose and will follow this with text.
  Enabled
  /// Text being composed, not yet committed, with the caret and the selection
  /// anchor as byte offsets into it. `-1` means the input method has no opinion.
  Preedit(String, cursor~ : Int32, anchor~ : Int32)
  /// Text that is settled and belongs in the document.
  Commit(String)
  /// The host is no longer composing.
  Disabled
}

///|
/// Something the host reports about a window.
///
/// Sizes, positions and gesture distances are in physical pixels; text and keys
/// are described by [`Key`]. A backend fills this in from whatever its platform
/// reports, so a consumer can be written once and run on every desktop.
#derive(Debug)
pub(all) enum Event {
  /// The drawable area changed size, in physical pixels.
  Resized(Int32, Int32)
  /// The window moved to a screen with a different scale factor, or the scale
  /// factor of its screen changed. Everything drawn from now on should be laid
  /// out again in the new logical size.
  ScaleChanged(Double)
  /// The window gained or lost the keyboard.
  Focused(Bool)
  /// The user asked for the window to go away: the close box, Alt+F4, a
  /// window-manager binding.
  CloseRequested
  /// A key went down.
  KeyDown(KeyEvent)
  /// A key came up.
  KeyUp(KeyEvent)
  /// Text to insert, already composed: a keystroke that produced a character,
  /// or a commit from an input method. Not the same thing as a key press — a
  /// dead key produces no text, a Chinese input method produces several
  /// characters from several keys.
  Text(String)
  /// The state of composition changed.
  Ime(Ime)
  /// The pointer moved, in physical pixels from the top-left of the client area.
  PointerMoved(Double, Double)
  /// The pointer entered the client area.
  PointerEntered
  /// The pointer left the client area.
  PointerLeft
  /// A mouse button went down.
  ButtonDown(MouseButton)
  /// A mouse button came up.
  ButtonUp(MouseButton)
  /// A wheel or a trackpad scrolled.
  Wheel(WheelDelta)
  /// A trackpad gesture.
  Gesture(Gesture)
}
