#ifndef GODOT_AUTOPILOT_DISPLAY_TOOLS_HPP
#define GODOT_AUTOPILOT_DISPLAY_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/display_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace display_tools {

namespace {

const std::vector<ParamSpec> kGetDisplayClipboardParams = {};

const std::vector<ParamSpec> kSetDisplayClipboardParams = {
    {"text", "string", "Text to write into the system clipboard, replacing its current contents", true},
};

const std::vector<ParamSpec> kShowDisplayDialogParams = {
    {"title", "string", "Title shown in the dialog's title bar", true},
    {"description", "string", "Message body shown inside the dialog", true},
    {"buttons", "array", "Array of button label strings, one button per entry, e.g. ['OK', 'Cancel']; note the dialog is fire-and-forget, so the clicked button is not reported back", true},
};

const std::vector<ParamSpec> kGetDisplayMousePositionParams = {};

const std::vector<ParamSpec> kSetDisplayMouseModeParams = {
    {"mode", "integer", "Cursor behavior: 0=visible, 1=hidden, 2=captured (pointer locked to the window, used for FPS controls), 3=confined, 4=confined_hidden; values outside 0-4 return an error", true},
};

const std::vector<ParamSpec> kWarpDisplayMouseParams = {
    {"x", "integer", "Mouse X position in pixels relative to the client area of the focused window (not screen coordinates), e.g. 640", true},
    {"y", "integer", "Mouse Y position in pixels relative to the client area of the focused window (not screen coordinates), e.g. 360", true},
};

const std::vector<ParamSpec> kCaptureDisplayScreenParams = {
    {"screen", "integer", "Index of the physical screen to capture, default 0; valid range is 0 to count-1 from get_display_screen_count", false},
};

const std::vector<ParamSpec> kGetDisplayScreenCountParams = {};

const std::vector<ParamSpec> kGetDisplayScreenDpiParams = {
    {"screen", "integer", "Index of the screen to query, default 0; valid range is 0 to count-1 from get_display_screen_count", false},
};

const std::vector<ParamSpec> kGetDisplayScreenPositionParams = {
    {"screen", "integer", "Index of the screen to query, default 0; valid range is 0 to count-1 from get_display_screen_count", false},
};

const std::vector<ParamSpec> kGetDisplayScreenRefreshRateParams = {
    {"screen", "integer", "Index of the screen to query, default 0; valid range is 0 to count-1 from get_display_screen_count", false},
};

const std::vector<ParamSpec> kGetDisplayScreenSizeParams = {
    {"screen", "integer", "Index of the screen to query, default 0; valid range is 0 to count-1 from get_display_screen_count", false},
};

const std::vector<ParamSpec> kGetDisplayTtsVoicesParams = {};

const std::vector<ParamSpec> kSpeakDisplayTtsParams = {
    {"text", "string", "Text to synthesize and speak aloud, e.g. 'Level complete'", true},
    {"voice", "string", "Voice id from get_display_tts_voices; omit to use the system default voice", false},
    {"volume", "integer", "Speech volume from 0 to 100, default 50", false},
    {"pitch", "number", "Speech pitch multiplier, default 1.0; values above 1.0 raise the pitch", false},
    {"rate", "number", "Speech speed multiplier, default 1.0; values above 1.0 speak faster", false},
};

const std::vector<ParamSpec> kStopDisplayTtsParams = {};

const std::vector<ParamSpec> kCreateDisplayWindowParams = {
    {"mode", "integer", "Window mode as a Window.Mode enum value, default 0 (windowed); note this enum differs from the DisplayServer.WindowMode used by set_display_window_mode", false},
    {"rect", "object", "Window rectangle as an object with integer x, y, w and h fields, e.g. {x: 100, y: 50, w: 800, h: 600}; defaults to 800x600 at the origin", false},
};

const std::vector<ParamSpec> kDeleteDisplayWindowParams = {
    {"window_id", "integer", "window_id as returned by create_display_window; errors when no window with this id exists", true},
};

const std::vector<ParamSpec> kGetDisplayWindowRectParams = {
    {"window_id", "integer", "Window id to query (default: 0 = main window)", false},
};

const std::vector<ParamSpec> kMoveDisplayWindowToForegroundParams = {
    {"window_id", "integer", "window_id of the window to raise, e.g. from create_display_window; defaults to the main window", false},
};

const std::vector<ParamSpec> kRequestDisplayWindowAttentionParams = {
    {"window_id", "integer", "window_id of the window to flash in the taskbar, e.g. from create_display_window; defaults to the main window", false},
};

const std::vector<ParamSpec> kSetDisplayWindowFlagParams = {
    {"flag", "integer", "Window flag as a DisplayServer.WindowFlags enum value, e.g. 0 for always-on-top or 1 for borderless", true},
    {"enabled", "boolean", "true applies the flag, false removes it", true},
    {"window_id", "integer", "window_id of the window to modify, e.g. from create_display_window; defaults to the main window", false},
};

const std::vector<ParamSpec> kSetDisplayWindowModeParams = {
    {"mode", "integer", "Window mode as a DisplayServer.WindowMode enum value, e.g. 1 for fullscreen or 2 for maximized; note this enum differs from the Window.Mode enum used by create_display_window", true},
    {"window_id", "integer", "window_id of the window to modify, e.g. from create_display_window; defaults to the main window", false},
};

const std::vector<ParamSpec> kSetDisplayWindowPositionParams = {
    {"x", "integer", "Window X position in screen coordinates (pixels)", true},
    {"y", "integer", "Window Y position in screen coordinates (pixels)", true},
    {"window_id", "integer", "window_id of the window to move, e.g. from create_display_window; defaults to the main window", false},
};

const std::vector<ParamSpec> kSetDisplayWindowSizeParams = {
    {"width", "integer", "Window width in pixels, e.g. 1024", true},
    {"height", "integer", "Window height in pixels, e.g. 768", true},
    {"window_id", "integer", "window_id of the window to resize, e.g. from create_display_window; defaults to the main window", false},
};

const std::vector<ParamSpec> kSetDisplayWindowTitleParams = {
    {"title", "string", "Title bar text to set, e.g. 'Output Preview'", true},
    {"window_id", "integer", "window_id of the window to retitle, e.g. from create_display_window; defaults to the main window", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(25);
  v.push_back(make_spec_tool(ToolSpec{
      "get_display_clipboard",
      "Read the current text contents of the system clipboard. Use it to retrieve text copied from other applications or previously set with set_display_clipboard. Returns result as a string; it may be empty when the clipboard holds no text or only non-text data.",
      "Display", {"display", "clipboard"}, SideEffect::None, tool_flags::kNone,
      kGetDisplayClipboardParams, display_ops::handle_clipboard_get}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_display_clipboard",
      "Write the given text into the system clipboard, replacing its current contents. Use it to share text with other applications or prepare text for pasting into the editor. Takes text; returns result 'ok'. The clipboard is global to the operating system and persists after the call.",
      "Display", {"display", "clipboard"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kSetDisplayClipboardParams, display_ops::handle_clipboard_set}));
  v.push_back(make_spec_tool(ToolSpec{
      "show_display_dialog",
      "Show a native modal dialog with a title, description and the given button labels using the operating system. Use it to prompt the user or surface a message outside the editor UI. Takes title, description and an array of button label strings. The dialog is fire-and-forget: an empty callback is passed, so the clicked button cannot be reported back to the caller.",
      "Display", {"display", "dialog"}, SideEffect::ShowsAlert, tool_flags::kMutating,
      kShowDisplayDialogParams, display_ops::handle_dialog_show}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_display_mouse_position",
      "Return the current mouse cursor position in screen (desktop) coordinates — a different basis from warp_display_mouse and the mouse input tools, whose positions are relative to the client area of the focused window. Use it to read the pointer's real desktop position, not as a drop-in input to those tools. Returns result as an object with integer x and y fields.",
      "Display", {"display", "mouse"}, SideEffect::None, tool_flags::kNone,
      kGetDisplayMousePositionParams, display_ops::handle_mouse_get_position}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_display_mouse_mode",
      "Set the mouse cursor behavior for the current display. Use it for first-person camera control (captured) or hiding the cursor during cutscenes; captured locks the pointer to the window. Takes mode: 0=visible, 1=hidden, 2=captured, 3=confined, 4=confined_hidden; values outside 0-4 return an error. Returns result 'ok'.",
      "Display", {"display", "mouse"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kSetDisplayMouseModeParams, display_ops::handle_mouse_set_mode}));
  v.push_back(make_spec_tool(ToolSpec{
      "warp_display_mouse",
      "Move the mouse cursor instantly to the given position, in pixels relative to the client area of the focused window — the engine converts the position onto the desktop — not screen coordinates, and not the basis reported by get_display_mouse_position. Use it to place the pointer before simulating clicks with the input tools, e.g. warp_display_mouse to the target, then move_input_mouse and press_input_mouse_button at the same window-client coordinates. Takes integer x and y. Returns result 'ok'; behavior may vary slightly between platforms.",
      "Display", {"display", "mouse"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kWarpDisplayMouseParams, display_ops::handle_mouse_warp}));
  v.push_back(make_spec_tool(ToolSpec{
      "capture_display_screen",
      "Capture the contents of a physical screen as a PNG image. Use it to take desktop screenshots or verify window placement visually. Takes screen (default 0). Returns result with mime, format and data fields, where data is a base64-encoded PNG. Unlike capture_editor_viewport (editor viewport) and capture_game_viewport (running game), this tool captures the whole physical display.",
      "Display", {"display", "screen"}, SideEffect::None, tool_flags::kCaptureImage,
      kCaptureDisplayScreenParams, display_ops::handle_screen_capture}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_display_screen_count",
      "Return the number of screens (monitors) connected to the system. Use it before addressing screens by index in other display tools, e.g. get_display_screen_size or capture_display_screen; valid indices run from 0 to count-1. Returns result as an integer, at least 1 on most systems.",
      "Display", {"display", "screen"}, SideEffect::None, tool_flags::kNone,
      kGetDisplayScreenCountParams, display_ops::handle_screen_get_count}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_display_screen_dpi",
      "Return the DPI (dots per inch) of the given screen. Use it to account for display density differences when sizing UI or windows. Takes screen (default 0). Returns result as an integer; some platforms report 0 when DPI data is unavailable.",
      "Display", {"display", "screen"}, SideEffect::None, tool_flags::kNone,
      kGetDisplayScreenDpiParams, display_ops::handle_screen_get_dpi}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_display_screen_position",
      "Return the position of the given screen's top-left corner in the desktop coordinate space. Use it to compute where windows should be placed or to understand the physical monitor layout. Takes screen (default 0). Returns result as an object with integer x and y fields.",
      "Display", {"display", "screen"}, SideEffect::None, tool_flags::kNone,
      kGetDisplayScreenPositionParams, display_ops::handle_screen_get_position}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_display_screen_refresh_rate",
      "Return the refresh rate of the given screen in hertz. Use it to tune or cap the rendering loop, or to verify a monitor's supported rate. Takes screen (default 0). Returns result as a number; values may be 0 when the rate cannot be determined.",
      "Display", {"display", "screen"}, SideEffect::None, tool_flags::kNone,
      kGetDisplayScreenRefreshRateParams, display_ops::handle_screen_get_refresh_rate}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_display_screen_size",
      "Return the pixel size of the given screen in pixels. Use it to size windows with set_display_window_size or to plan capture dimensions. Takes screen (default 0). Returns result as an object with integer x (width) and y (height) fields, matching the screen resolution.",
      "Display", {"display", "screen"}, SideEffect::None, tool_flags::kNone,
      kGetDisplayScreenSizeParams, display_ops::handle_screen_get_size}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_display_tts_voices",
      "List the text-to-speech voices available on the system. Use it before speaking with speak_display_tts to pick a suitable voice; each returned voice has an id field that can be passed as the voice argument. Returns result as an array of voice objects whose fields vary by platform.",
      "Display", {"display", "tts"}, SideEffect::None, tool_flags::kNone,
      kGetDisplayTtsVoicesParams, display_ops::handle_tts_get_voices}));
  v.push_back(make_spec_tool(ToolSpec{
      "speak_display_tts",
      "Speak the given text aloud using the system text-to-speech engine. Use it for accessibility or audible feedback in the project. Optionally pass a voice id from get_display_tts_voices, volume 0-100 (default 50), pitch (default 1.0) and rate (default 1.0). Speech is asynchronous; interrupt it with stop_display_tts. Returns result 'ok'.",
      "Display", {"display", "tts"}, SideEffect::ShowsAlert, tool_flags::kMutating,
      kSpeakDisplayTtsParams, display_ops::handle_tts_speak}));
  v.push_back(make_spec_tool(ToolSpec{
      "stop_display_tts",
      "Stop any ongoing text-to-speech playback immediately. Use it to interrupt speech started with speak_display_tts, e.g. when new text should be spoken or the user is done listening. Returns result 'ok' even when nothing is currently being spoken, making it safe to call unconditionally.",
      "Display", {"display", "tts"}, SideEffect::ShowsAlert, tool_flags::kMutating,
      kStopDisplayTtsParams, display_ops::handle_tts_stop}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_display_window",
      "Create a new sub-window in the editor process and show it on screen. Use it to display auxiliary content, e.g. a preview or dashboard. Returns result as the window_id, which the window tools (set_display_window_mode, set_display_window_size, delete_display_window and others) accept as their window_id argument. Optional mode (a Window.Mode enum value) and rect {x, y, w, h}; the rect defaults to 800x600 at the origin. Errors when the platform does not support subwindows.",
      "Display", {"display", "window"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kCreateDisplayWindowParams, display_ops::handle_window_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "delete_display_window",
      "Close and free a sub-window previously created with create_display_window. Use it to clean up windows that are no longer needed. Takes window_id as returned by create_display_window; errors when no window with the given id exists. Returns result 'ok'; the window is freed at the end of the frame.",
      "Display", {"display", "window"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kDeleteDisplayWindowParams, display_ops::handle_window_delete}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_display_window_rect",
      "Return the client-area rectangle of the main window (or of the window given by window_id) in virtual desktop coordinates, together with the geometry and scale of the screen it is on. Use it to place or compare windows and to translate between desktop points and window-client coordinates before injecting input: warp_display_mouse, move_input_mouse and press_input_mouse_button expect coordinates relative to the focused window's client area, so subtract position from a desktop point (or add position to client-area coordinates). Takes optional window_id (default: 0, the main window); negative ids return an error. Returns result as an object with window_id, position {x, y}, size {x, y} where x is the width and y the height, decorated_position, decorated_size, screen, screen_scale, screen_position, screen_size and a note field. Read-only; it does not modify the window.",
      "Display", {"display", "window", "geometry"}, SideEffect::None, tool_flags::kNone,
      kGetDisplayWindowRectParams, display_ops::handle_window_get_rect}));
  v.push_back(make_spec_tool(ToolSpec{
      "move_display_window_to_foreground",
      "Raise a window to the foreground of the desktop and give it focus. Use it to bring a sub-window to attention over the editor, e.g. right after creating it. Takes window_id, which defaults to the main window when omitted. Returns result 'ok'.",
      "Display", {"display", "window"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kMoveDisplayWindowToForegroundParams, display_ops::handle_window_move_to_foreground}));
  v.push_back(make_spec_tool(ToolSpec{
      "request_display_window_attention",
      "Flash the window's taskbar entry or otherwise attract the user's attention without stealing focus from the user. Use it to signal that a background window needs attention, e.g. after a long-running operation finishes. Takes window_id, which defaults to the main window when omitted. Returns result 'ok'.",
      "Display", {"display", "window"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kRequestDisplayWindowAttentionParams, display_ops::handle_window_request_attention}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_display_window_flag",
      "Set a window flag on the given window, controlling behavior such as always-on-top, borderless or resizability. Use it to customize a sub-window's appearance. Takes flag as a DisplayServer.WindowFlags enum value, enabled (boolean) and optional window_id (default: main window). Returns result 'ok'.",
      "Display", {"display", "window"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kSetDisplayWindowFlagParams, display_ops::handle_window_set_flag}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_display_window_mode",
      "Set the window mode of the given window, e.g. fullscreen, minimized or maximized. Use it to change how a window is displayed on screen. Takes mode as a DisplayServer.WindowMode enum value; note that this enum differs from the Window.Mode enum used by create_display_window. Takes optional window_id (default: main window). Returns result 'ok'.",
      "Display", {"display", "window"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kSetDisplayWindowModeParams, display_ops::handle_window_set_mode}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_display_window_position",
      "Move the given window so its top-left corner is at the specified screen coordinates. Use it to arrange sub-windows on the desktop, e.g. for side-by-side layouts. Takes integer x and y plus optional window_id (default: main window). Returns result 'ok'.",
      "Display", {"display", "window"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kSetDisplayWindowPositionParams, display_ops::handle_window_set_position}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_display_window_size",
      "Resize the given window to the specified pixel dimensions. Use it to set a fixed window size, e.g. to match a resolution or aspect ratio. Takes width and height in pixels plus optional window_id (default: main window). Returns result 'ok'.",
      "Display", {"display", "window"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kSetDisplayWindowSizeParams, display_ops::handle_window_set_size}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_display_window_title",
      "Set the title bar text of the given window. Use it to label a window meaningfully, e.g. right after create_display_window, so the user can identify it in the taskbar. Takes title and optional window_id (default: main window). Returns result 'ok'.",
      "Display", {"display", "window"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kSetDisplayWindowTitleParams, display_ops::handle_window_set_title}));
  return v;
}

} // namespace display_tools
} // namespace godot_autopilot

#endif