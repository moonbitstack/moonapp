///|
/// The scale factor a screen reports at a given resolution.
///
/// A desktop defines one scale step as 96 dots per inch, so a 144 dpi screen is
/// one and a half. Windows reports dpi, X11 leaves it to the `Xft.dpi` resource
/// or the screen's own millimetres, Wayland sends a fixed-point scale, and macOS
/// sends nothing at all but changes its coordinate space instead — each backend
/// turns that into this number, and a host that reports nothing is taken to be
/// at the base resolution.
///
/// A resolution of zero or less is not a resolution, and becomes one.
pub fn scale_from_dpi(dpi : Double) -> Double {
  if dpi <= 0.0 {
    1.0
  } else {
    dpi / 96.0
  }
}

///|
/// The same length in physical pixels: what the frame is drawn in.
pub fn to_physical(logical : Double, scale : Double) -> Double {
  logical * valid_scale(scale)
}

///|
/// The same length in logical pixels: what the user thinks in, and what a
/// layout is written in.
pub fn to_logical(physical : Double, scale : Double) -> Double {
  physical / valid_scale(scale)
}

///|
/// A scale factor a host reported, with a nonsense one replaced by the base.
fn valid_scale(scale : Double) -> Double {
  if scale > 0.0 {
    scale
  } else {
    1.0
  }
}

///|
/// A length in physical pixels, rounded to the nearest one and never negative.
/// A host converts a physical length to whole pixels with this before handing it
/// to a platform, so that two calls that should agree do.
pub fn to_pixel(length : Double) -> Int32 {
  let value = length.round()
  if value < 0.0 {
    0
  } else if value > 2147483647.0 {
    2147483647
  } else {
    value.to_int()
  }
}

///|
/// A physical length that is at least one pixel, for sizes that would otherwise
/// come out as nothing at all on a small scale.
pub fn to_extent(length : Double) -> Int32 {
  let pixels = to_pixel(length)
  if pixels < 1 {
    1
  } else {
    pixels
  }
}
