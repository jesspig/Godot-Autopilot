#ifndef GODOT_AUTOPILOT_DISPLAY_TOOLS_HPP
#define GODOT_AUTOPILOT_DISPLAY_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/display_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace display_tools {

GDA_TOOL_CLASS(GetDisplayClipboardTool, "get_display_clipboard",
               "Read the current text contents of the system clipboard. Use it to retrieve text copied from other applications or previously set with set_display_clipboard. Returns result as a string; it may be empty when the clipboard holds no text or only non-text data.",
               "Display", std::vector<std::string>({"display", "clipboard"}), display_ops::handle_clipboard_get, false)

GDA_TOOL_CLASS_SIDE(SetDisplayClipboardTool, "set_display_clipboard",
               "Write the given text into the system clipboard, replacing its current contents. Use it to share text with other applications or prepare text for pasting into the editor. Takes text; returns result 'ok'. The clipboard is global to the operating system and persists after the call.",
               "Display", std::vector<std::string>({"display", "clipboard"}), display_ops::handle_clipboard_set, false, ::godot_autopilot::SideEffect::ModifiesWindow)

GDA_TOOL_CLASS_SIDE(ShowDisplayDialogTool, "show_display_dialog",
               "Show a native modal dialog with a title, description and the given button labels using the operating system. Use it to prompt the user or surface a message outside the editor UI. Takes title, description and an array of button label strings. The dialog is fire-and-forget: an empty callback is passed, so the clicked button cannot be reported back to the caller.",
               "Display", std::vector<std::string>({"display", "dialog"}), display_ops::handle_dialog_show, false, ::godot_autopilot::SideEffect::ShowsAlert)

GDA_TOOL_CLASS(GetDisplayMousePositionTool, "get_display_mouse_position",
               "Return the current mouse cursor position in screen (desktop) coordinates — a different basis from warp_display_mouse and the mouse input tools, whose positions are relative to the client area of the focused window. Use it to read the pointer's real desktop position, not as a drop-in input to those tools. Returns result as an object with integer x and y fields.",
               "Display", std::vector<std::string>({"display", "mouse"}), display_ops::handle_mouse_get_position, false)

GDA_TOOL_CLASS_SIDE(SetDisplayMouseModeTool, "set_display_mouse_mode",
               "Set the mouse cursor behavior for the current display. Use it for first-person camera control (captured) or hiding the cursor during cutscenes; captured locks the pointer to the window. Takes mode: 0=visible, 1=hidden, 2=captured, 3=confined, 4=confined_hidden; values outside 0-4 return an error. Returns result 'ok'.",
               "Display", std::vector<std::string>({"display", "mouse"}), display_ops::handle_mouse_set_mode, false, ::godot_autopilot::SideEffect::ModifiesWindow)

GDA_TOOL_CLASS_SIDE(WarpDisplayMouseTool, "warp_display_mouse",
               "Move the mouse cursor instantly to the given position, in pixels relative to the client area of the focused window — the engine converts the position onto the desktop — not screen coordinates, and not the basis reported by get_display_mouse_position. Use it to place the pointer before simulating clicks with the input tools, e.g. warp_display_mouse to the target, then move_input_mouse and press_input_mouse_button at the same window-client coordinates. Takes integer x and y. Returns result 'ok'; behavior may vary slightly between platforms.",
               "Display", std::vector<std::string>({"display", "mouse"}), display_ops::handle_mouse_warp, false, ::godot_autopilot::SideEffect::ModifiesWindow)

GDA_TOOL_CLASS(CaptureDisplayScreenTool, "capture_display_screen",
               "Capture the contents of a physical screen as a PNG image. Use it to take desktop screenshots or verify window placement visually. Takes screen (default 0). Returns result with mime, format and data fields, where data is a base64-encoded PNG. Unlike capture_editor_viewport (editor viewport) and capture_game_viewport (running game), this tool captures the whole physical display.",
               "Display", std::vector<std::string>({"display", "screen"}), display_ops::handle_screen_capture, false)

GDA_TOOL_CLASS(GetDisplayScreenCountTool, "get_display_screen_count",
               "Return the number of screens (monitors) connected to the system. Use it before addressing screens by index in other display tools, e.g. get_display_screen_size or capture_display_screen; valid indices run from 0 to count-1. Returns result as an integer, at least 1 on most systems.",
               "Display", std::vector<std::string>({"display", "screen"}), display_ops::handle_screen_get_count, false)

GDA_TOOL_CLASS(GetDisplayScreenDpiTool, "get_display_screen_dpi",
               "Return the DPI (dots per inch) of the given screen. Use it to account for display density differences when sizing UI or windows. Takes screen (default 0). Returns result as an integer; some platforms report 0 when DPI data is unavailable.",
               "Display", std::vector<std::string>({"display", "screen"}), display_ops::handle_screen_get_dpi, false)

GDA_TOOL_CLASS(GetDisplayScreenPositionTool, "get_display_screen_position",
               "Return the position of the given screen's top-left corner in the desktop coordinate space. Use it to compute where windows should be placed or to understand the physical monitor layout. Takes screen (default 0). Returns result as an object with integer x and y fields.",
               "Display", std::vector<std::string>({"display", "screen"}), display_ops::handle_screen_get_position, false)

GDA_TOOL_CLASS(GetDisplayScreenRefreshRateTool, "get_display_screen_refresh_rate",
               "Return the refresh rate of the given screen in hertz. Use it to tune or cap the rendering loop, or to verify a monitor's supported rate. Takes screen (default 0). Returns result as a number; values may be 0 when the rate cannot be determined.",
               "Display", std::vector<std::string>({"display", "screen"}), display_ops::handle_screen_get_refresh_rate, false)

GDA_TOOL_CLASS(GetDisplayScreenSizeTool, "get_display_screen_size",
               "Return the pixel size of the given screen in pixels. Use it to size windows with set_display_window_size or to plan capture dimensions. Takes screen (default 0). Returns result as an object with integer x (width) and y (height) fields, matching the screen resolution.",
               "Display", std::vector<std::string>({"display", "screen"}), display_ops::handle_screen_get_size, false)

GDA_TOOL_CLASS(GetDisplayTtsVoicesTool, "get_display_tts_voices",
               "List the text-to-speech voices available on the system. Use it before speaking with speak_display_tts to pick a suitable voice; each returned voice has an id field that can be passed as the voice argument. Returns result as an array of voice objects whose fields vary by platform.",
               "Display", std::vector<std::string>({"display", "tts"}), display_ops::handle_tts_get_voices, false)

GDA_TOOL_CLASS_SIDE(SpeakDisplayTtsTool, "speak_display_tts",
               "Speak the given text aloud using the system text-to-speech engine. Use it for accessibility or audible feedback in the project. Optionally pass a voice id from get_display_tts_voices, volume 0-100 (default 50), pitch (default 1.0) and rate (default 1.0). Speech is asynchronous; interrupt it with stop_display_tts. Returns result 'ok'.",
               "Display", std::vector<std::string>({"display", "tts"}), display_ops::handle_tts_speak, false, ::godot_autopilot::SideEffect::ShowsAlert)

GDA_TOOL_CLASS_SIDE(StopDisplayTtsTool, "stop_display_tts",
               "Stop any ongoing text-to-speech playback immediately. Use it to interrupt speech started with speak_display_tts, e.g. when new text should be spoken or the user is done listening. Returns result 'ok' even when nothing is currently being spoken, making it safe to call unconditionally.",
               "Display", std::vector<std::string>({"display", "tts"}), display_ops::handle_tts_stop, false, ::godot_autopilot::SideEffect::ShowsAlert)

GDA_TOOL_CLASS_SIDE(CreateDisplayWindowTool, "create_display_window",
               "Create a new sub-window in the editor process and show it on screen. Use it to display auxiliary content, e.g. a preview or dashboard. Returns result as the window_id, which the window tools (set_display_window_mode, set_display_window_size, delete_display_window and others) accept as their window_id argument. Optional mode (a Window.Mode enum value) and rect {x, y, w, h}; the rect defaults to 800x600 at the origin. Errors when the platform does not support subwindows.",
               "Display", std::vector<std::string>({"display", "window"}), display_ops::handle_window_create, false, ::godot_autopilot::SideEffect::ModifiesWindow)

GDA_TOOL_CLASS_SIDE(DeleteDisplayWindowTool, "delete_display_window",
               "Close and free a sub-window previously created with create_display_window. Use it to clean up windows that are no longer needed. Takes window_id as returned by create_display_window; errors when no window with the given id exists. Returns result 'ok'; the window is freed at the end of the frame.",
               "Display", std::vector<std::string>({"display", "window"}), display_ops::handle_window_delete, false, ::godot_autopilot::SideEffect::ModifiesWindow)

GDA_TOOL_CLASS_SIDE(MoveDisplayWindowToForegroundTool, "move_display_window_to_foreground",
               "Raise a window to the foreground of the desktop and give it focus. Use it to bring a sub-window to attention over the editor, e.g. right after creating it. Takes window_id, which defaults to the main window when omitted. Returns result 'ok'.",
               "Display", std::vector<std::string>({"display", "window"}), display_ops::handle_window_move_to_foreground, false, ::godot_autopilot::SideEffect::ModifiesWindow)

GDA_TOOL_CLASS_SIDE(RequestDisplayWindowAttentionTool, "request_display_window_attention",
               "Flash the window's taskbar entry or otherwise attract the user's attention without stealing focus from the user. Use it to signal that a background window needs attention, e.g. after a long-running operation finishes. Takes window_id, which defaults to the main window when omitted. Returns result 'ok'.",
               "Display", std::vector<std::string>({"display", "window"}), display_ops::handle_window_request_attention, false, ::godot_autopilot::SideEffect::ModifiesWindow)

GDA_TOOL_CLASS_SIDE(SetDisplayWindowFlagTool, "set_display_window_flag",
               "Set a window flag on the given window, controlling behavior such as always-on-top, borderless or resizability. Use it to customize a sub-window's appearance. Takes flag as a DisplayServer.WindowFlags enum value, enabled (boolean) and optional window_id (default: main window). Returns result 'ok'.",
               "Display", std::vector<std::string>({"display", "window"}), display_ops::handle_window_set_flag, false, ::godot_autopilot::SideEffect::ModifiesWindow)

GDA_TOOL_CLASS_SIDE(SetDisplayWindowModeTool, "set_display_window_mode",
               "Set the window mode of the given window, e.g. fullscreen, minimized or maximized. Use it to change how a window is displayed on screen. Takes mode as a DisplayServer.WindowMode enum value; note that this enum differs from the Window.Mode enum used by create_display_window. Takes optional window_id (default: main window). Returns result 'ok'.",
               "Display", std::vector<std::string>({"display", "window"}), display_ops::handle_window_set_mode, false, ::godot_autopilot::SideEffect::ModifiesWindow)

GDA_TOOL_CLASS_SIDE(SetDisplayWindowPositionTool, "set_display_window_position",
               "Move the given window so its top-left corner is at the specified screen coordinates. Use it to arrange sub-windows on the desktop, e.g. for side-by-side layouts. Takes integer x and y plus optional window_id (default: main window). Returns result 'ok'.",
               "Display", std::vector<std::string>({"display", "window"}), display_ops::handle_window_set_position, false, ::godot_autopilot::SideEffect::ModifiesWindow)

GDA_TOOL_CLASS_SIDE(SetDisplayWindowSizeTool, "set_display_window_size",
               "Resize the given window to the specified pixel dimensions. Use it to set a fixed window size, e.g. to match a resolution or aspect ratio. Takes width and height in pixels plus optional window_id (default: main window). Returns result 'ok'.",
               "Display", std::vector<std::string>({"display", "window"}), display_ops::handle_window_set_size, false, ::godot_autopilot::SideEffect::ModifiesWindow)

GDA_TOOL_CLASS_SIDE(SetDisplayWindowTitleTool, "set_display_window_title",
               "Set the title bar text of the given window. Use it to label a window meaningfully, e.g. right after create_display_window, so the user can identify it in the taskbar. Takes title and optional window_id (default: main window). Returns result 'ok'.",
               "Display", std::vector<std::string>({"display", "window"}), display_ops::handle_window_set_title, false, ::godot_autopilot::SideEffect::ModifiesWindow)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(24);
  v.push_back(std::make_unique<GetDisplayClipboardTool>());
  v.push_back(std::make_unique<SetDisplayClipboardTool>());
  v.push_back(std::make_unique<ShowDisplayDialogTool>());
  v.push_back(std::make_unique<GetDisplayMousePositionTool>());
  v.push_back(std::make_unique<SetDisplayMouseModeTool>());
  v.push_back(std::make_unique<WarpDisplayMouseTool>());
  v.push_back(std::make_unique<CaptureDisplayScreenTool>());
  v.push_back(std::make_unique<GetDisplayScreenCountTool>());
  v.push_back(std::make_unique<GetDisplayScreenDpiTool>());
  v.push_back(std::make_unique<GetDisplayScreenPositionTool>());
  v.push_back(std::make_unique<GetDisplayScreenRefreshRateTool>());
  v.push_back(std::make_unique<GetDisplayScreenSizeTool>());
  v.push_back(std::make_unique<GetDisplayTtsVoicesTool>());
  v.push_back(std::make_unique<SpeakDisplayTtsTool>());
  v.push_back(std::make_unique<StopDisplayTtsTool>());
  v.push_back(std::make_unique<CreateDisplayWindowTool>());
  v.push_back(std::make_unique<DeleteDisplayWindowTool>());
  v.push_back(std::make_unique<MoveDisplayWindowToForegroundTool>());
  v.push_back(std::make_unique<RequestDisplayWindowAttentionTool>());
  v.push_back(std::make_unique<SetDisplayWindowFlagTool>());
  v.push_back(std::make_unique<SetDisplayWindowModeTool>());
  v.push_back(std::make_unique<SetDisplayWindowPositionTool>());
  v.push_back(std::make_unique<SetDisplayWindowSizeTool>());
  v.push_back(std::make_unique<SetDisplayWindowTitleTool>());
  return v;
}

} // namespace display_tools
} // namespace godot_autopilot

#endif