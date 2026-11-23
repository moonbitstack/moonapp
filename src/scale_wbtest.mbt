///|
test "a desktop resolution becomes the scale factor every screen but the first uses" {
  assert_eq(scale_from_dpi(96.0), 1.0)
  assert_eq(scale_from_dpi(144.0), 1.5)
  assert_eq(scale_from_dpi(192.0), 2.0)
  assert_eq(scale_from_dpi(120.0), 1.25)
}

///|
test "a nonsense resolution is the base one" {
  assert_eq(scale_from_dpi(0.0), 1.0)
  assert_eq(scale_from_dpi(-96.0), 1.0)
}

///|
test "logical and physical lengths convert both ways" {
  assert_eq(to_physical(100.0, 2.0), 200.0)
  assert_eq(to_logical(200.0, 2.0), 100.0)
  assert_eq(to_physical(100.0, 1.5), 150.0)
  assert_eq(to_logical(150.0, 1.5), 100.0)
  assert_eq(to_physical(100.0, 1.0), 100.0)
  // A scale a host forgot to report is the base one, not a division by zero.
  assert_eq(to_physical(100.0, 0.0), 100.0)
  assert_eq(to_logical(100.0, 0.0), 100.0)
}

///|
test "a length becomes whole pixels the same way twice" {
  assert_eq(to_pixel(0.4), 0)
  assert_eq(to_pixel(0.5), 1)
  assert_eq(to_pixel(1.5), 2)
  assert_eq(to_pixel(2.5), 3)
  assert_eq(to_pixel(-3.0), 0)
  assert_eq(to_pixel(2147483648.0), 2147483647)
}

///|
test "a size never comes out as nothing" {
  assert_eq(to_extent(0.4), 1)
  assert_eq(to_extent(0.0), 1)
  assert_eq(to_extent(240.2), 240)
}
