///|
/// A colour with straight (not premultiplied) alpha, each channel from 0 to 255.
#derive(Debug, Eq)
pub(all) struct Rgba {
  r : UInt32
  g : UInt32
  b : UInt32
  a : UInt32
}

///|
/// A colour from four channels, each taken modulo 256.
pub fn Rgba::new(r : Int32, g : Int32, b : Int32, a : Int32) -> Rgba {
  { r: channel(r), g: channel(g), b: channel(b), a: channel(a), }
}

///|
/// A colour that hides whatever is behind it.
pub fn Rgba::solid(r : Int32, g : Int32, b : Int32) -> Rgba {
  Rgba::new(r, g, b, 255)
}

///|
fn channel(value : Int32) -> UInt32 {
  (value & 0xFF).reinterpret_as_uint()
}

///|
pub let rgba_black : Rgba = { r: 0, g: 0, b: 0, a: 255, }

///|
pub let rgba_white : Rgba = { r: 255, g: 255, b: 255, a: 255, }

///|
pub let rgba_transparent : Rgba = { r: 0, g: 0, b: 0, a: 0, }

///|
fn put(pixels : FixedArray[Byte], at : Int32, value : UInt32) -> Unit {
  pixels[at] = value.reinterpret_as_int().to_byte()
}

///|
fn get(pixels : FixedArray[Byte], at : Int32) -> UInt32 {
  (pixels[at].to_int() & 0xFF).reinterpret_as_uint()
}

///|
/// A rectangle of pixels in RGBA, eight bits per channel, straight alpha, first
/// row at the top. This is what a renderer produces and what the host puts on
/// screen; each backend converts it to its own byte order on the way.
#derive(Debug)
pub(all) struct Frame {
  width : Int32
  height : Int32
  pixels : FixedArray[Byte]
}

///|
/// An empty frame of the given size, every pixel transparent.
///
/// A size that is not positive gives a zero-sized frame rather than an error:
/// a window being resized or hidden passes through such sizes, and a renderer
/// should be able to keep running.
pub fn Frame::new(width : Int32, height : Int32) -> Frame {
  let w = if width > 0 { width } else { 0 }
  let h = if height > 0 { height } else { 0 }
  { width: w, height: h, pixels: FixedArray::make(w * h * 4, b'\x00'), }
}

///|
/// A frame that takes over an existing buffer of RGBA bytes.
///
/// # Panics
///
/// If the buffer is not exactly `4 * width * height` bytes long.
pub fn Frame::from_rgba(
  width : Int32,
  height : Int32,
  pixels : FixedArray[Byte],
) -> Frame {
  let want = width * height * 4
  if pixels.length() != want {
    abort("Frame::from_rgba: \{pixels.length()} bytes for \{width}x\{height}")
  }
  { width, height, pixels, }
}

///|
/// Whether the frame has no pixels at all.
pub fn Frame::is_empty(self : Frame) -> Bool {
  self.width <= 0 || self.height <= 0
}

///|
fn Frame::offset(self : Frame, x : Int32, y : Int32) -> Int32 {
  (y * self.width + x) * 4
}

///|
/// The colour at a pixel.
///
/// # Panics
///
/// If the pixel is outside the frame.
pub fn Frame::get(self : Frame, x : Int32, y : Int32) -> Rgba {
  let at = self.offset(x, y)
  {
    r: get(self.pixels, at),
    g: get(self.pixels, at + 1),
    b: get(self.pixels, at + 2),
    a: get(self.pixels, at + 3),
  }
}

///|
/// The colour at a pixel, or transparent black outside the frame, so a reader
/// that samples around an edge does not have to clamp itself.
pub fn Frame::at(self : Frame, x : Int32, y : Int32) -> Rgba {
  if x < 0 || y < 0 || x >= self.width || y >= self.height {
    rgba_transparent
  } else {
    self.get(x, y)
  }
}

///|
/// Paint a pixel, replacing what is there.
pub fn Frame::set(self : Frame, x : Int32, y : Int32, color : Rgba) -> Unit {
  let at = self.offset(x, y)
  put(self.pixels, at, color.r)
  put(self.pixels, at + 1, color.g)
  put(self.pixels, at + 2, color.b)
  put(self.pixels, at + 3, color.a)
}

///|
/// Paint a rectangle, clipped to the frame.
pub fn Frame::fill_rect(
  self : Frame,
  x : Int32,
  y : Int32,
  width : Int32,
  height : Int32,
  color : Rgba,
) -> Unit {
  let right = if x + width < self.width { x + width } else { self.width }
  let bottom = if y + height < self.height { y + height } else { self.height }
  for row in y..<bottom {
    for column in x..<right {
      self.set(column, row, color)
    }
  }
}

///|
/// Paint every pixel of the frame.
pub fn Frame::fill(self : Frame, color : Rgba) -> Unit {
  self.fill_rect(0, 0, self.width, self.height, color)
}

///|
/// A channel multiplied by an alpha, rounded to the nearest integer.
fn premultiplied(value : UInt32, alpha : UInt32) -> Int32 {
  (value.reinterpret_as_int() * alpha.reinterpret_as_int() + 127) / 255
}

///|
/// The frame's bytes in the order a platform wants them.
///
/// `blue_first` swaps red and blue, which is what the desktops want.
/// `premultiply` multiplies the colour channels by the alpha, which is what
/// Wayland and CoreGraphics expect; a host that stores the buffer as it is
/// given wants straight alpha. `top_down` puts the first row at the top, as the
/// frame has it; a platform that numbers rows from the bottom passes `false`.
pub fn Frame::to_host_order(
  self : Frame,
  blue_first~ : Bool = true,
  premultiply~ : Bool = false,
  top_down~ : Bool = true,
) -> FixedArray[Byte] {
  let out = FixedArray::make(self.pixels.length(), b'\x00')
  let row = self.width * 4
  for y in 0..<self.height {
    let from = y * row
    let to = if top_down { y * row } else { (self.height - 1 - y) * row }
    for x in 0..<self.width {
      let i = from + x * 4
      let r = get(self.pixels, i)
      let g = get(self.pixels, i + 1)
      let b = get(self.pixels, i + 2)
      let a = get(self.pixels, i + 3)
      let (r, g, b) = if premultiply {
        (
          channel(premultiplied(r, a)),
          channel(premultiplied(g, a)),
          channel(premultiplied(b, a)),
        )
      } else {
        (r, g, b)
      }
      let at = to + x * 4
      if blue_first {
        put(out, at, b)
        put(out, at + 1, g)
        put(out, at + 2, r)
      } else {
        put(out, at, r)
        put(out, at + 1, g)
        put(out, at + 2, b)
      }
      put(out, at + 3, a)
    }
  }
  out
}

///|
/// Read a buffer a platform produced into a frame.
///
/// The inverse of [`Frame::to_host_order`]: `stride` is the row length in bytes,
/// which is usually `4 * width` but need not be, because platforms pad their
/// rows. `blue_first` and `top_down` describe the buffer.
pub fn Frame::from_host_order(
  width : Int32,
  height : Int32,
  stride : Int32,
  buffer : FixedArray[Byte],
  blue_first~ : Bool = true,
  top_down~ : Bool = true,
) -> Frame {
  let frame = Frame::new(width, height)
  for y in 0..<height {
    let from = (if top_down { y } else { height - 1 - y }) * stride
    for x in 0..<width {
      let i = from + x * 4
      let first = get(buffer, i)
      let second = get(buffer, i + 1)
      let third = get(buffer, i + 2)
      let a = get(buffer, i + 3)
      let (r, b) = if blue_first { (third, first) } else { (first, third) }
      frame.set(x, y, { r, g: second, b, a, })
    }
  }
  frame
}

///|
/// A buffer of `4 * width * height` bytes for a platform to write a window into.
pub fn frame_buffer(width : Int32, height : Int32) -> FixedArray[Byte] {
  FixedArray::make(width * height * 4, b'\x00')
}
