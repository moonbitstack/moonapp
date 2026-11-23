///|
test "a new frame is transparent black" {
  let frame = Frame::new(3, 2)
  assert_eq(frame.width, 3)
  assert_eq(frame.height, 2)
  assert_eq(frame.pixels.length(), 3 * 2 * 4)
  assert_eq(frame.get(2, 1), rgba_transparent)
}

///|
test "a frame that is asked for no room has none" {
  let frame = Frame::new(-4, 0)
  assert_eq(frame.is_empty(), true)
  assert_eq(frame.pixels.length(), 0)
  assert_eq(Frame::new(0, 0).is_empty(), true)
  assert_eq(Frame::new(1, 1).is_empty(), false)
}

///|
test "pixels land where they are put" {
  let frame = Frame::new(2, 2)
  frame.set(1, 0, Rgba::new(1, 2, 3, 4))
  assert_eq(frame.get(1, 0), Rgba::new(1, 2, 3, 4))
  assert_eq(frame.get(0, 0), rgba_transparent)
  assert_eq(frame.get(0, 1), rgba_transparent)
  // Outside the frame reads as nothing, so an edge sample does not panic.
  assert_eq(frame.at(5, 5), rgba_transparent)
  assert_eq(frame.at(-1, 0), rgba_transparent)
}

///|
test "channels wrap rather than overflow a byte" {
  assert_eq(Rgba::new(256, 257, -1, 300), Rgba::new(0, 1, 255, 44))
  assert_eq(Rgba::new(0, 0, 0, 255).a, 255)
}

///|
test "filling a frame paints every pixel" {
  let frame = Frame::new(2, 3)
  frame.fill(Rgba::solid(10, 20, 30))
  for y in 0..<3 {
    for x in 0..<2 {
      assert_eq(frame.get(x, y), Rgba::solid(10, 20, 30))
    }
  }
}

///|
test "a filled rectangle is clipped to the frame" {
  let frame = Frame::new(3, 3)
  frame.fill_rect(1, 1, 5, 5, rgba_white)
  assert_eq(frame.get(0, 0), rgba_transparent)
  assert_eq(frame.get(1, 1), rgba_white)
  assert_eq(frame.get(2, 2), rgba_white)
}

///|
test "the host order of a frame swaps red and blue" {
  let frame = Frame::new(1, 1)
  frame.set(0, 0, Rgba::new(1, 2, 3, 4))
  let bgra = frame.to_host_order()
  assert_eq(bgra[0].to_int(), 3)
  assert_eq(bgra[1].to_int(), 2)
  assert_eq(bgra[2].to_int(), 1)
  assert_eq(bgra[3].to_int(), 4)
  let rgba = frame.to_host_order(blue_first=false)
  assert_eq(rgba[0].to_int(), 1)
  assert_eq(rgba[2].to_int(), 3)
}

///|
test "premultiplying scales the colour by the alpha" {
  let frame = Frame::new(1, 1)
  frame.set(0, 0, Rgba::new(200, 100, 50, 128))
  let premultiplied = frame.to_host_order(premultiply=true)
  // 200 * 128 / 255 = 100.4, rounded to 100; 100 * 128 / 255 = 50.2; 50 * 128 / 255 = 25.1.
  assert_eq(premultiplied[0].to_int(), 25)
  assert_eq(premultiplied[1].to_int(), 50)
  assert_eq(premultiplied[2].to_int(), 100)
  // A transparent pixel has nothing left in its colour.
  frame.set(0, 0, Rgba::new(200, 100, 50, 0))
  let clear = frame.to_host_order(premultiply=true)
  assert_eq(clear[0].to_int(), 0)
  assert_eq(clear[2].to_int(), 0)
}

///|
test "a bottom-up buffer comes out the other way up" {
  let frame = Frame::new(2, 2)
  frame.set(0, 0, Rgba::new(1, 2, 3, 17))
  frame.set(0, 1, Rgba::new(4, 5, 6, 34))
  let flipped = frame.to_host_order(top_down=false)
  // Alpha says which row a pixel came from, whatever the channel order is.
  assert_eq(flipped[3].to_int(), 34)
  assert_eq(flipped[11].to_int(), 17)
}

///|
test "a buffer read back from a host is the frame that went out" {
  let frame = Frame::new(4, 3)
  for y in 0..<3 {
    for x in 0..<4 {
      frame.set(x, y, Rgba::new(x * 10, y * 20, 7, 255))
    }
  }
  let out = frame.to_host_order()
  let back = Frame::from_host_order(4, 3, 16, out)
  for y in 0..<3 {
    for x in 0..<4 {
      assert_eq(back.get(x, y), frame.get(x, y))
    }
  }
}

///|
test "a padded row is read at its own stride" {
  let buffer = FixedArray::make(2 * 20, b'\x00')
  // Row 0: red at the first pixel, row 1: blue at the first pixel of its padded row.
  buffer[0] = b'\x00'
  buffer[1] = b'\x00'
  buffer[2] = (255 : Byte)
  buffer[3] = (255 : Byte)
  buffer[20] = (255 : Byte)
  buffer[21] = b'\x00'
  buffer[22] = b'\x00'
  buffer[23] = (255 : Byte)
  let frame = Frame::from_host_order(2, 2, 20, buffer)
  assert_eq(frame.get(0, 0), Rgba::solid(255, 0, 0))
  assert_eq(frame.get(0, 1), Rgba::solid(0, 0, 255))
  assert_eq(frame.get(1, 1), Rgba::new(0, 0, 0, 0))
}

///|
test "panic frame from a buffer of the wrong length" {
  let _ = Frame::from_rgba(2, 2, FixedArray::make(4, b'\x00'))
}
