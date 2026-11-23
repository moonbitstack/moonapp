// A window with a switch on it: the pointer moves the highlight, a click flips
// the colour, and the keys and the wheel are printed as they arrive. It closes
// itself after a couple of seconds so that a machine without a person in front
// of it finishes.

///|
let panel : @moonapp.WindowConfig = {
  ..@moonapp.window_config,
  title: "moonapp",
  width: 480,
  height: 320,
}

///|
fn paint(
  width : Int32,
  height : Int32,
  pointer : (Double, Double),
  on : Bool,
  pressed : Bool,
) -> @moonapp.Frame {
  let frame = @moonapp.Frame::new(width, height)
  frame.fill(@moonapp.Rgba::solid(24, 26, 32))
  let scale = width.to_double() / 480.0
  let margin = (24.0 * scale).to_int()
  let bar = (56.0 * scale).to_int()
  // A running bar of colour, so a frame that is only partly drawn shows.
  for x in 0..<(width - margin * 2) {
    let t = x.to_double() / (width - margin * 2).to_double()
    frame.fill_rect(
      x + margin,
      margin,
      (2.0 * scale).to_int() + 1,
      bar,
      @moonapp.Rgba::solid(
        (40.0 + 200.0 * t).to_int(),
        90,
        (200.0 - 150.0 * t).to_int(),
      ),
    )
  }
  // The switch: green when on, grey when off, lighter while pressed.
  let (x, y) = (margin, margin * 2 + bar)
  let switch_width = (140.0 * scale).to_int()
  let switch_height = (64.0 * scale).to_int()
  let base = if on {
    @moonapp.Rgba::solid(60, 200, 120)
  } else {
    @moonapp.Rgba::solid(70, 74, 84)
  }
  let lit = if pressed { @moonapp.Rgba::solid(90, 230, 150) } else { base }
  frame.fill_rect(x, y, switch_width, switch_height, lit)
  let knob = (52.0 * scale).to_int()
  let knob_x = if on {
    x + switch_width - knob - margin / 2
  } else {
    x + margin / 2
  }
  frame.fill_rect(
    knob_x,
    y + margin / 4,
    knob,
    knob,
    @moonapp.Rgba::solid(240, 240, 245),
  )
  // A dot where the pointer is.
  let (px, py) = pointer
  let radius = (10.0 * scale).to_int()
  let cx = px.to_int()
  let cy = py.to_int()
  for dy in -radius..<radius {
    for dx in -radius..<radius {
      let (x, y) = (cx + dx, cy + dy)
      if dx * dx + dy * dy <= radius * radius &&
        x >= 0 &&
        y >= 0 &&
        x < width &&
        y < height {
        frame.set(x, y, @moonapp.Rgba::new(250, 220, 60, 200))
      }
    }
  }
  frame
}

///|
fn inside(
  pointer : (Double, Double),
  switch_at : (Int32, Int32),
  size : (Int32, Int32),
) -> Bool {
  let (x, y) = pointer
  let (left, top) = switch_at
  let (width, height) = size
  x >= left.to_double() &&
  y >= top.to_double() &&
  x < (left + width).to_double() &&
  y < (top + height).to_double()
}

///|
fn run() -> Unit raise @moonapp.AppError {
  let app = @host.App::new()
  let window = app.open(window_config=panel)
  let mut on = false
  let mut pressed = false
  let mut pointer : (Double, Double) = (0.0, 0.0)
  let mut frames = 0
  let mut running = true
  while running {
    let (width, height) = window.size()
    let scale = width.to_double() / 480.0
    let switch_at = (
      (24.0 * scale).to_int(),
      (24.0 * 2.0 * scale + 56.0 * scale).to_int(),
    )
    let switch_size = ((140.0 * scale).to_int(), (64.0 * scale).to_int())
    window.present(paint(width, height, pointer, on, pressed))
    frames = frames + 1
    if frames > 120 {
      running = false
    }
    match app.wait(timeout=16) {
      Some((_, @moonapp.CloseRequested)) => running = false
      Some((_, @moonapp.KeyDown(key))) => {
        println("key down: \{key.key.name()}")
        if key.key == @moonapp.Key::Escape {
          running = false
        }
      }
      Some((_, @moonapp.KeyUp(key))) => println("key up: \{key.key.name()}")
      Some((_, @moonapp.Text(text))) => println("text: \{text}")
      Some((_, @moonapp.PointerMoved(x, y))) => {
        pointer = (x, y)
        let over = inside(pointer, switch_at, switch_size)
        window.set_cursor(
          if over {
            @moonapp.Pointer
          } else {
            @moonapp.Default
          },
        )
      }
      Some((_, @moonapp.ButtonDown(@moonapp.Left))) =>
        if inside(pointer, switch_at, switch_size) {
          pressed = true
          on = !on
          println("switch: \{if on { "on" } else { "off" }}")
        }
      Some((_, @moonapp.ButtonUp(_))) => pressed = false
      Some((_, @moonapp.Wheel(delta))) => println("wheel: \{delta.y}")
      Some((_, @moonapp.Resized(width, height))) =>
        println("resized: \{width} by \{height}")
      Some((_, @moonapp.ScaleChanged(scale))) => println("scale: \{scale}")
      Some((_, @moonapp.Ime(@moonapp.Commit(text)))) =>
        println("composed: \{text}")
      _ => ()
    }
  }
  window.close()
  app.close()
}

///|
fn main {
  let outcome = Ok(run()) catch { error => Err(error) }
  match outcome {
    Ok(_) => ()
    Err(@moonapp.Unavailable(reason)) => println("no desktop: \{reason}")
    Err(@moonapp.Refused(reason)) => println("the desktop refused: \{reason}")
    Err(@moonapp.Malformed(reason)) =>
      println("the desktop answered wrongly: \{reason}")
  }
}
