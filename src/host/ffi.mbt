///|
/// The entry points every desktop backend implements, under the same names.
///
/// One C file per desktop fills these in; which one is compiled in is decided by
/// the preprocessor guard at the top of the file. Nothing above this file knows
/// which desktop it is running on, which is why the rest of the package has no
/// per-platform branches at all.
#external
pub type Host

///|
extern "C" fn host_new() -> Host = "moonapp_host_new"

///|
extern "C" fn host_ok(host : Host) -> Int32 = "moonapp_host_ok"

///|
extern "C" fn host_free(host : Host) = "moonapp_host_free"

///|
#borrow(into)
extern "C" fn host_error(
  host : Host,
  into : FixedArray[Byte],
  cap : Int32,
) -> Int32 = "moonapp_host_error"

///|
#borrow(title, app_id)
extern "C" fn open_window(
  host : Host,
  title : Bytes,
  title_len : Int32,
  app_id : Bytes,
  app_id_len : Int32,
  width : Int32,
  height : Int32,
  flags : Int32,
) -> Int32 = "moonapp_open"

///|
extern "C" fn primary_scale(host : Host) -> Double = "moonapp_primary_scale"

///|
extern "C" fn is_shutdown(host : Host) -> Int32 = "moonapp_is_shutdown"

///|
extern "C" fn close_window(host : Host, window : Int32) = "moonapp_close"

///|
extern "C" fn window_is_open(host : Host, window : Int32) -> Int32 = "moonapp_window_open"

///|
extern "C" fn shutdown(host : Host) = "moonapp_host_shutdown"

///|
extern "C" fn poll(host : Host) -> Int32 = "moonapp_poll"

///|
extern "C" fn wait(host : Host, timeout_ms : Int32) -> Int32 = "moonapp_wait"

///|
extern "C" fn event_release(host : Host) = "moonapp_event_release"

///|
extern "C" fn event_kind(host : Host) -> Int32 = "moonapp_event_kind"

///|
extern "C" fn event_window(host : Host) -> Int32 = "moonapp_event_window"

///|
extern "C" fn event_i32(host : Host, which : Int32) -> Int32 = "moonapp_event_i32"

///|
extern "C" fn event_f64(host : Host, which : Int32) -> Double = "moonapp_event_f64"

///|
extern "C" fn event_text_len(host : Host) -> Int32 = "moonapp_event_text_len"

///|
#borrow(buffer)
extern "C" fn event_text(
  host : Host,
  buffer : FixedArray[Byte],
  cap : Int32,
) -> Int32 = "moonapp_event_text"

///|
#borrow(title)
extern "C" fn set_title(
  host : Host,
  window : Int32,
  title : Bytes,
  len : Int32,
) = "moonapp_set_title"

///|
extern "C" fn set_size(
  host : Host,
  window : Int32,
  width : Int32,
  height : Int32,
) = "moonapp_set_size"

///|
extern "C" fn set_min_size(
  host : Host,
  window : Int32,
  width : Int32,
  height : Int32,
) = "moonapp_set_min_size"

///|
extern "C" fn width_of(host : Host, window : Int32) -> Int32 = "moonapp_width"

///|
extern "C" fn height_of(host : Host, window : Int32) -> Int32 = "moonapp_height"

///|
extern "C" fn scale_of(host : Host, window : Int32) -> Double = "moonapp_scale"

///|
extern "C" fn set_cursor(host : Host, window : Int32, cursor : Int32) = "moonapp_set_cursor"

///|
#borrow(bgra)
extern "C" fn present(
  host : Host,
  window : Int32,
  bgra : FixedArray[Byte],
  width : Int32,
  height : Int32,
  stride : Int32,
) -> Int32 = "moonapp_present"

///|
#borrow(buffer)
extern "C" fn read_back(
  host : Host,
  window : Int32,
  buffer : FixedArray[Byte],
  cap : Int32,
) -> Int32 = "moonapp_read_back"

///|
#borrow(buffer)
extern "C" fn clipboard_get(
  host : Host,
  buffer : FixedArray[Byte],
  cap : Int32,
) -> Int32 = "moonapp_clipboard_get"

///|
#borrow(text)
extern "C" fn clipboard_set(host : Host, text : Bytes, len : Int32) = "moonapp_clipboard_set"

///|
extern "C" fn set_ime_allowed(host : Host, window : Int32, allowed : Int32) = "moonapp_set_ime_allowed"

///|
extern "C" fn set_ime_area(
  host : Host,
  window : Int32,
  x : Int32,
  y : Int32,
  width : Int32,
  height : Int32,
) = "moonapp_set_ime_area"

///|
extern "C" fn native_kind() -> Int32 = "moonapp_native_kind"

///|
extern "C" fn native_display(host : Host) -> UInt64 = "moonapp_native_display"

///|
extern "C" fn native_window(host : Host, window : Int32) -> UInt64 = "moonapp_native_window"
