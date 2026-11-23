// The tests in this file drive a real window on the desktop they run on. A
// machine without a session to open one in — a headless runner that did not set
// up a display — skips them; a run that declares itself live fails instead, so a
// green result means a window was really opened.

///|
extern "C" fn test_key(
  host : Host,
  window : Int32,
  code : Int32,
  down : Int32,
) -> Int32 = "moonapp_test_key"

///|
extern "C" fn test_text(host : Host, window : Int32, unit : Int32) -> Int32 = "moonapp_test_text"

///|
extern "C" fn test_pointer(
  host : Host,
  window : Int32,
  kind : Int32,
  x : Int32,
  y : Int32,
  button : Int32,
) -> Int32 = "moonapp_test_pointer"

///|
fn live() -> Bool {
  @env.get_env_var("MOONAPP_LIVE") is Some(_)
}

///|
fn desktop() -> App? {
  let attempted = Ok(App::new()) catch { error => Err(error) }
  match attempted {
    Ok(app) => Some(app)
    Err(_) => {
      if live() {
        abort("MOONAPP_LIVE is set but the desktop refused a connection")
      }
      None
    }
  }
}

///|
/// Somewhere for a test to keep what it has seen.
struct Rig {
  app : App
  window : Window
  host : Host
  id : Int32
  width : Int32
  height : Int32
}

///|
fn rig(config : @moonapp.WindowConfig) -> Rig? {
  guard desktop() is Some(app) else { return None }
  let attempted = Ok(app.open(window_config=config)) catch {
    error => Err(error)
  }
  match attempted {
    Err(_) => {
      if live() {
        abort("the desktop refused the window")
      }
      None
    }
    Ok(window) => {
      let (width, height) = window.size()
      Some({
        app,
        window,
        host: app.host,
        id: window.id().value,
        width,
        height,
      })
    }
  }
}

///|
/// Wait for the next event that `wanted` accepts, up to a second and a half.
/// Tests do not race the desktop: an event that has not arrived yet is waited
/// for, and one that never arrives fails the test rather than looping forever.
fn expect_event(
  rig : Rig,
  wanted : (@moonapp.Event) -> Bool,
) -> @moonapp.Event? {
  for _ in 0..<15 {
    guard rig.app.wait(timeout=100) is Some((_, event)) else { continue }
    if wanted(event) {
      return Some(event)
    }
  }
  None
}

///|
/// A frame of four quadrant colours, so a read-back says both that the pixels
/// arrived and that they arrived the right way up and in the right order.
fn quadrant_frame(width : Int32, height : Int32) -> @moonapp.Frame {
  let frame = @moonapp.Frame::new(width, height)
  let half_width = width / 2
  let half_height = height / 2
  frame.fill_rect(
    0,
    0,
    half_width,
    half_height,
    @moonapp.Rgba::solid(255, 0, 0),
  )
  frame.fill_rect(
    half_width,
    0,
    width - half_width,
    half_height,
    @moonapp.Rgba::solid(0, 255, 0),
  )
  frame.fill_rect(
    0,
    half_height,
    half_width,
    height - half_height,
    @moonapp.Rgba::solid(0, 0, 255),
  )
  frame.fill_rect(
    half_width,
    half_height,
    width - half_width,
    height - half_height,
    @moonapp.Rgba::solid(255, 255, 255),
  )
  frame
}

///|
test "a frame put on a window reads back from it" {
  guard rig(@moonapp.window_config) is Some(rig) else { return }
  let frame = quadrant_frame(rig.width, rig.height)
  rig.window.present(frame)
  // Wayland has no request for a window to read itself back; the backend says
  // so rather than inventing one.
  if rig.app.platform() == @moonapp.Wayland {
    let refused = Ok(rig.window.read_back()) catch { error => Err(error) }
    assert_eq(refused is Err(_), true)
    rig.window.close()
    rig.app.close()
    return
  }
  let back = rig.window.read_back() catch { _ => abort("read_back failed") }
  assert_eq(back.width, rig.width)
  assert_eq(back.height, rig.height)
  let quarter = (x, y) => {
    back.get(
      rig.width / 2 * x + rig.width / 4,
      rig.height / 2 * y + rig.height / 4,
    )
  }
  assert_eq(quarter(0, 0), @moonapp.Rgba::solid(255, 0, 0))
  assert_eq(quarter(1, 0), @moonapp.Rgba::solid(0, 255, 0))
  assert_eq(quarter(0, 1), @moonapp.Rgba::solid(0, 0, 255))
  assert_eq(quarter(1, 1), @moonapp.Rgba::solid(255, 255, 255))
  rig.window.close()
  rig.app.close()
}

///|
test "the window reports the size it was asked for and the screen's scale" {
  let config : @moonapp.WindowConfig = {
    ..@moonapp.window_config,
    title: "size",
    width: 200,
    height: 120,
    visible: false,
  }
  guard rig(config) is Some(rig) else { return }
  let scale = rig.window.scale()
  assert_eq(scale > 0.0, true)
  // The desktop may add a title bar, so only the client area is compared, and
  // only after the window has told us what it settled on.
  assert_eq(rig.width, @moonapp.to_extent(@moonapp.to_physical(200.0, scale)))
  assert_eq(rig.height, @moonapp.to_extent(@moonapp.to_physical(120.0, scale)))
  rig.window.close()
  rig.app.close()
}

///|
test "a key press and its text reach the application" {
  guard rig(@moonapp.window_config) is Some(rig) else { return }
  let code = match rig.app.platform() {
    @moonapp.Windows => 0x41
    @moonapp.Macos => 0x00
    _ => 0x61
  }
  // A compositor does not let one client put events into another's queue, so
  // there is nothing to drive on Wayland.
  if test_key(rig.host, rig.id, code, 1) == 0 {
    rig.window.close()
    rig.app.close()
    return
  }
  let down = expect_event(rig, fn(event) {
    match event {
      @moonapp.KeyDown(key) =>
        key.key == @moonapp.Key::from_platform(rig.app.platform(), code)
      _ => false
    }
  })
  assert_eq(down is Some(_), true)
  // X11 and Wayland make the text out of the key press itself; the other two
  // desktops send it as a message of its own, which is what the hook posts.
  let expected = match rig.app.platform() {
    @moonapp.Windows | @moonapp.Macos => {
      let _ = test_text(rig.host, rig.id, 0x41)
      "A"
    }
    _ => "a"
  }
  let text = expect_event(rig, fn(event) {
    match event {
      @moonapp.Text(text) => text == expected
      _ => false
    }
  })
  assert_eq(text is Some(_), true)
  let _ = test_key(rig.host, rig.id, code, 0)
  let up = expect_event(rig, fn(event) {
    match event {
      @moonapp.KeyUp(key) =>
        key.key == @moonapp.Key::from_platform(rig.app.platform(), code)
      _ => false
    }
  })
  assert_eq(up is Some(_), true)
  rig.window.close()
  rig.app.close()
}

///|
test "the pointer and the wheel reach the application" {
  guard rig(@moonapp.window_config) is Some(rig) else { return }
  if test_pointer(rig.host, rig.id, 0, 40, 30, 0) == 0 {
    rig.window.close()
    rig.app.close()
    return
  }
  let moved = expect_event(rig, fn(event) {
    match event {
      @moonapp.PointerMoved(x, y) => x == 40.0 && y == 30.0
      _ => false
    }
  })
  assert_eq(moved is Some(_), true)
  let _ = test_pointer(rig.host, rig.id, 1, 40, 30, 0)
  let pressed = expect_event(rig, fn(event) {
    match event {
      @moonapp.ButtonDown(@moonapp.Left) => true
      _ => false
    }
  })
  assert_eq(pressed is Some(_), true)
  let _ = test_pointer(rig.host, rig.id, 2, 40, 30, 0)
  let released = expect_event(rig, fn(event) {
    match event {
      @moonapp.ButtonUp(@moonapp.Left) => true
      _ => false
    }
  })
  assert_eq(released is Some(_), true)
  let _ = test_pointer(rig.host, rig.id, 5, 0, 1, 0)
  let wheeled = expect_event(rig, fn(event) {
    match event {
      @moonapp.Wheel(delta) => delta.y == 1.0
      _ => false
    }
  })
  assert_eq(wheeled is Some(_), true)
  rig.window.close()
  rig.app.close()
}

///|
test "the clipboard gives back what was put on it" {
  guard rig(@moonapp.window_config) is Some(rig) else { return }
  // The clipboard belongs to the whole desktop, so this test puts back what it
  // found rather than leaving its own text behind.
  let before = rig.window.clipboard() catch { _ => "" }
  let text = "moonapp: 剪贴板"
  rig.window.set_clipboard(text)
  let after = rig.window.clipboard() catch { _ => "" }
  assert_eq(after, text)
  rig.window.set_clipboard(before)
  rig.window.close()
  rig.app.close()
}

///|
test "the title changes and the window goes away" {
  guard rig(@moonapp.window_config) is Some(rig) else { return }
  rig.window.set_title("renamed")
  assert_eq(rig.window.is_open(), true)
  rig.window.close()
  assert_eq(rig.window.is_open(), false)
  assert_eq(rig.window.size(), (0, 0))
  rig.app.close()
}

///|
test "the surface names the object a GPU backend would attach to" {
  guard rig(@moonapp.window_config) is Some(rig) else { return }
  let surface = rig.window.surface()
  assert_eq(surface.second != 0UL, true)
  assert_eq(surface.kind != @moonapp.None, true)
  rig.window.close()
  rig.app.close()
}
