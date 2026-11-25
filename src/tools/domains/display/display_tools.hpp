#ifndef GODOT_AUTOPILOT_DISPLAY_TOOLS_HPP
#define GODOT_AUTOPILOT_DISPLAY_TOOLS_HPP

#include <initializer_list>
#include "util/json_value.hpp"
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/display/display_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace display_tools {

namespace {

const std::vector<ParamSpec> kGetDisplayInfoParams = {
    {"screen", "integer", "Only with action=screens: restrict the screens array to this screen index (default: every screen); the returned entry keeps its index field", false},
    {"window_id", "integer", "Only with action=window: window id to query (default: 0 = main window); negative ids return an error", false},
};

const std::vector<ParamSpec> kSetDisplayClipboardParams = {
    {"text", "string", "Text to write into the system clipboard, replacing its current contents", true},
};

const std::vector<ParamSpec> kShowDisplayDialogParams = {
    {"title", "string", "Title shown in the dialog's title bar", true},
    {"description", "string", "Message body shown inside the dialog", true},
    {"buttons", "array", "Array of button label strings, one button per entry, e.g. ['OK', 'Cancel']; note the dialog is fire-and-forget, so the clicked button is not reported back", true},
};

const std::vector<ParamSpec> kCaptureDisplayScreenParams = {
    {"screen", "integer", "Index of the physical screen to capture, default 0; valid range is 0 to count-1 from get_display_info action=screens", false},
};

const std::vector<ParamSpec> kDeleteDisplayWindowParams = {
    {"window_id", "integer", "window_id as returned by display_window action=create; errors when no window with this id exists, and window_id 0 (the editor main window) is refused", true},
};

const std::vector<ParamSpec> kDisplayWindowParams = {
    {"window_id", "integer", "window_id of the target sub-window; defaults to the main window for foreground, attention, flag, mode, position, size and title, and is ignored by create. popup, popup_centered, transient, exclusive, unparent_when_invisible and close require a sub-window created with action=create; window_id 0 is refused", false},
    {"mode", "integer", "Window mode integer: for create it is a Window.Mode value (default 0 windowed); for the mode action it is a DisplayServer.WindowMode value (e.g. 1 fullscreen, 2 maximized) — the two enums differ", false},
    {"rect", "object", "Only with action=create or action=popup: window rectangle as {x, y, w, h} integers; create defaults to 800x600 at the origin, popup requires w and h above 0", false},
    {"x", "integer", "Only with action=position: window X position in screen coordinates (pixels)", false},
    {"y", "integer", "Only with action=position: window Y position in screen coordinates (pixels)", false},
    {"width", "integer", "Only with action=size (window width in pixels, e.g. 1024) or action=popup_centered (minimum width, default 0)", false},
    {"height", "integer", "Only with action=size (window height in pixels, e.g. 768) or action=popup_centered (minimum height, default 0)", false},
    {"title", "string", "Only with action=title: title bar text to set, e.g. 'Output Preview'", false},
    {"flag", "integer", "Only with action=flag: Window flag as a DisplayServer.WindowFlags enum value, e.g. 0 for always-on-top or 1 for borderless", false},
    {"enabled", "boolean", "Only with action=flag: true applies the flag, false removes it", false},
    {"transient", "boolean", "Only with action=transient: true marks the sub-window as transient of its parent (it follows the parent and stays above it), false clears the flag", false},
    {"exclusive", "boolean", "Only with action=exclusive: true makes the transient sub-window exclusive (it blocks input to its parent until hidden), false clears the flag", false},
    {"unparent_when_invisible", "boolean", "Only with action=unparent_when_invisible: true removes the sub-window from the tree when it becomes invisible (parents may re-add it)", false},
};

const std::vector<ParamSpec> kDisplayMouseParams = {
    {"mode", "integer", "Only with action=mode: cursor behavior 0=visible, 1=hidden, 2=captured (pointer locked to the window, used for FPS controls), 3=confined, 4=confined_hidden; values outside 0-4 return an error", false},
    {"x", "integer", "Only with action=warp: mouse X position in pixels relative to the client area of the focused window (not screen coordinates), e.g. 640", false},
    {"y", "integer", "Only with action=warp: mouse Y position in pixels relative to the client area of the focused window (not screen coordinates), e.g. 360", false},
};

const std::vector<ParamSpec> kDisplayTtsParams = {
    {"text", "string", "Only with action=speak: text to synthesize and speak aloud, e.g. 'Level complete'", false},
    {"voice", "string", "Only with action=speak: voice id from get_display_info action=voices; omit to use the system default voice", false},
    {"volume", "integer", "Only with action=speak: speech volume from 0 to 100, default 50", false},
    {"pitch", "number", "Only with action=speak: speech pitch multiplier, default 1.0; values above 1.0 raise the pitch", false},
    {"rate", "number", "Only with action=speak: speech speed multiplier, default 1.0; values above 1.0 speak faster", false},
};

const std::vector<std::string> kGetDisplayInfoActions = {
    "screens", "clipboard", "mouse", "voices", "window"};
const std::vector<std::string> kDisplayWindowActions = {
    "create", "foreground", "attention", "flag", "mode", "position", "size",
    "title", "popup", "popup_centered", "transient", "exclusive",
    "unparent_when_invisible", "close"};
const std::vector<std::string> kDisplayMouseActions = {"mode", "warp"};
const std::vector<std::string> kDisplayTtsActions = {
    "speak", "stop", "pause", "resume", "is_speaking"};

inline godot_autopilot::util::JsonValue action_args(const godot_autopilot::util::JsonValue &args, const char *action,
                                  std::initializer_list<const char *> keys) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  out["action"] = godot_autopilot::util::JsonValue(action);
  if (!args.IsObject()) {
    return out;
  }
  for (const char *key : keys) {
    const godot_autopilot::util::JsonValue *value = args.Find(key);
    if (value != nullptr) {
      out[key] = *value;
    }
  }
  return out;
}

inline const godot_autopilot::util::JsonValue *screen_entry(const godot_autopilot::util::JsonValue &result,
                                          const godot_autopilot::util::JsonValue &args) {
  const godot_autopilot::util::JsonValue *inner = result.Find("result");
  const godot_autopilot::util::JsonValue *screens =
      inner != nullptr && inner->IsObject() ? inner->Find("screens") : nullptr;
  if (screens == nullptr || !screens->IsArray()) {
    return nullptr;
  }
  int64_t requested = 0;
  const godot_autopilot::util::JsonValue *screen = args.Find("screen");
  if (screen != nullptr && screen->IsInt()) {
    requested = screen->GetInt();
  }
  for (const auto &entry : screens->GetArray()) {
    const godot_autopilot::util::JsonValue *index = entry.Find("index");
    if (index != nullptr && index->IsInt() && index->GetInt() == requested) {
      return &entry;
    }
  }
  return nullptr;
}

inline godot_autopilot::util::JsonValue project_screen_count(const godot_autopilot::util::JsonValue &result) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  const godot_autopilot::util::JsonValue *inner = result.Find("result");
  const godot_autopilot::util::JsonValue *count =
      inner != nullptr && inner->IsObject() ? inner->Find("count") : nullptr;
  if (count != nullptr) {
    out["result"] = *count;
  } else {
    out["error"] = godot_autopilot::util::JsonValue("screen count not available");
  }
  return out;
}

inline godot_autopilot::util::JsonValue project_screen_field(const godot_autopilot::util::JsonValue &result,
                                           const godot_autopilot::util::JsonValue &args,
                                           const char *field) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  const godot_autopilot::util::JsonValue *entry = screen_entry(result, args);
  const godot_autopilot::util::JsonValue *value =
      entry != nullptr ? entry->Find(field) : nullptr;
  if (value != nullptr) {
    out["result"] = *value;
  } else {
    out["error"] = godot_autopilot::util::JsonValue(
        "screen index out of range: pass 0 to count-1 as reported by "
        "get_display_info action=screens");
  }
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(8);
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "get_display_info",
      "Read display and input-device state in one read-only tool. action selects the view: 'screens' returns {count, screens:[{index, dpi, position:{x,y}, size:{x,y}, usable_rect:{x,y,w,h}, scale, max_scale, orientation, refresh_rate}]} for every connected screen (usable_rect is the work area excluding taskbars/docks, scale is the per-screen content scale, max_scale is the system maximum, orientation is a DisplayServer.ScreenOrientation value); 'clipboard' returns the current system clipboard text as result; 'mouse' returns the desktop mouse cursor position as {x, y} in screen coordinates (a different basis from display_mouse action=warp and the input mouse tools, which use focused-window client coordinates); 'voices' returns the system text-to-speech voices as an array whose fields vary by platform, each with an id usable as voice in display_tts action=speak; 'window' returns the client-area rectangle of the main window or the window given by window_id together with its screen geometry, scale and vsync (window_id, position, size, decorated_position, decorated_size, screen, screen_scale, screen_position, screen_size, vsync). Example: {\"action\": \"screens\"} or {\"action\": \"window\", \"window_id\": 0}.",
      "Display", {"display", "screen", "clipboard", "mouse", "tts", "voices", "window", "geometry", "scale", "orientation", "vsync"},
      SideEffect::None, tool_flags::kNone,
      kGetDisplayInfoParams, display_ops::handle_info_get,
      schema::build_action_schema(
          kGetDisplayInfoActions, kGetDisplayInfoParams,
          "screens: all connected screens with dpi, position, size, usable_rect, scale, max_scale, orientation and refresh rate; clipboard: clipboard text; mouse: desktop cursor position; voices: text-to-speech voices; window: window client rectangle, screen geometry and vsync"),
      {},
      {"get_display_info action=screens",
       "get_display_info action=window window_id=0"}},
      {
          ToolAlias{
              "get_display_screen_count", "get_display_info",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "screens", {});
              },
              project_screen_count, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_display_screen_dpi", "get_display_info",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "screens", {"screen"});
              },
              {}, "0.4.0", "0.6.0", {},
              [](const godot_autopilot::util::JsonValue &result, const godot_autopilot::util::JsonValue &args) {
                return project_screen_field(result, args, "dpi");
              }},
          ToolAlias{
              "get_display_screen_position", "get_display_info",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "screens", {"screen"});
              },
              {}, "0.4.0", "0.6.0", {},
              [](const godot_autopilot::util::JsonValue &result, const godot_autopilot::util::JsonValue &args) {
                return project_screen_field(result, args, "position");
              }},
          ToolAlias{
              "get_display_screen_refresh_rate", "get_display_info",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "screens", {"screen"});
              },
              {}, "0.4.0", "0.6.0", {},
              [](const godot_autopilot::util::JsonValue &result, const godot_autopilot::util::JsonValue &args) {
                return project_screen_field(result, args, "refresh_rate");
              }},
          ToolAlias{
              "get_display_screen_size", "get_display_info",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "screens", {"screen"});
              },
              {}, "0.4.0", "0.6.0", {},
              [](const godot_autopilot::util::JsonValue &result, const godot_autopilot::util::JsonValue &args) {
                return project_screen_field(result, args, "size");
              }},
          ToolAlias{
              "get_display_clipboard", "get_display_info",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "clipboard", {});
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_display_mouse_position", "get_display_info",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "mouse", {});
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_display_tts_voices", "get_display_info",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "voices", {});
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_display_window_rect", "get_display_info",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "window", {"window_id"});
              },
              {}, "0.4.0", "0.6.0"},
      })));
  v.push_back(make_spec_tool(ToolSpec{
      "set_display_clipboard",
      "Write the given text into the system clipboard, replacing its current contents. Use it to share text with other applications or prepare text for pasting into the editor. Takes text; returns result 'ok'. The clipboard is global to the operating system and persists after the call. Example: {\"text\": \"hello\"}.",
      "Display", {"display", "clipboard", "set"},
      SideEffect::ModifiesWindow, tool_flags::kMutating,
      kSetDisplayClipboardParams, display_ops::handle_clipboard_set,
      {}, {}, {"{\"text\": \"hello\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "show_display_dialog",
      "Show a native modal dialog with a title, description and the given button labels using the operating system. Use it to prompt the user or surface a message outside the editor UI. Takes title, description and an array of button label strings. The dialog is fire-and-forget: an empty callback is passed, so the clicked button cannot be reported back to the caller. Example: {\"title\": \"Build\", \"description\": \"Done\", \"buttons\": [\"OK\"]}.",
      "Display", {"display", "dialog", "alert"},
      SideEffect::ShowsAlert, tool_flags::kMutating,
      kShowDisplayDialogParams, display_ops::handle_dialog_show,
      {}, {}, {"{\"title\": \"Build\", \"description\": \"Done\", \"buttons\": [\"OK\"]}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "capture_display_screen",
      "Capture the contents of a physical screen as a PNG image. Use it to take desktop screenshots or verify window placement visually. Takes screen (default 0). Returns result with mime, format and data fields, where data is a base64-encoded PNG. Unlike capture_editor_viewport (editor viewport) and capture_game_viewport (running game), this tool captures the whole physical display. Example: {\"screen\": 0}.",
      "Display", {"display", "screen", "capture"},
      SideEffect::None, tool_flags::kCaptureImage,
      kCaptureDisplayScreenParams, display_ops::handle_screen_capture,
      {}, {}, {"{\"screen\": 0}"}}));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "display_window",
      "Create or modify an on-screen window in the editor process. action selects the operation (all ModifiesWindow, no confirmation): 'create' returns result window_id and shows a new sub-window parented under the editor main window (optional mode as a Window.Mode value and rect {x, y, w, h}, default 800x600 at the origin; errors when the platform does not support subwindows); 'foreground' raises and focuses the window; 'attention' flashes the taskbar entry without stealing focus; 'flag' applies or removes a DisplayServer.WindowFlags value with enabled; 'mode' sets a DisplayServer.WindowMode value (note: a different enum from create's Window.Mode); 'position' moves the window to screen coordinates x, y; 'size' resizes it to width x height pixels; 'title' sets the title bar text; 'popup' shows a sub-window at rect {x, y, w, h} with a non-modal Window.popup call; 'popup_centered' shows a sub-window centered on its parent with an optional minimum width/height; 'transient' set_transient toggles whether the sub-window follows its parent; 'exclusive' set_exclusive blocks parent input while the sub-window is visible (keep it transient); 'unparent_when_invisible' toggles removing the sub-window from the tree when hidden; 'close' hides the sub-window without freeing it. window_id defaults to the main window for foreground, attention, flag, mode, position, size and title, while popup, popup_centered, transient, exclusive, unparent_when_invisible and close require a sub-window created by 'create' and refuse window_id 0. Deleting a window is a destructive operation and stays in delete_display_window (requires confirm). Example: {\"action\": \"create\", \"rect\": {\"x\": 100, \"y\": 50, \"w\": 800, \"h\": 600}} or {\"action\": \"popup_centered\", \"window_id\": 2}.",
      "Display", {"display", "window", "create", "move", "resize", "title", "mode", "flag", "popup", "transient", "exclusive", "close"},
      SideEffect::ModifiesWindow, tool_flags::kMutating,
      kDisplayWindowParams, display_ops::handle_window_control,
      schema::build_action_schema(
          kDisplayWindowActions, kDisplayWindowParams,
          "create: new sub-window, returns window_id; foreground: raise and focus; attention: flash the taskbar entry; flag: apply or remove a WindowFlags value; mode: set a DisplayServer.WindowMode; position: move to x, y; size: resize to width x height; title: set the title bar text; popup: show the sub-window at rect; popup_centered: show centered with optional min width/height; transient, exclusive, unparent_when_invisible: toggle the matching Window flag; close: hide the sub-window"),
      {},
      {"display_window action=create",
       "display_window action=popup_centered window_id=2",
       "display_window action=title title=Preview window_id=0"}},
      {
          ToolAlias{
              "create_display_window", "display_window",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "create", {"mode", "rect"});
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "move_display_window_to_foreground", "display_window",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "foreground", {"window_id"});
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "request_display_window_attention", "display_window",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "attention", {"window_id"});
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_display_window_flag", "display_window",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "flag",
                                   {"flag", "enabled", "window_id"});
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_display_window_mode", "display_window",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "mode", {"mode", "window_id"});
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_display_window_position", "display_window",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "position", {"x", "y", "window_id"});
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_display_window_size", "display_window",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "size",
                                   {"width", "height", "window_id"});
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_display_window_title", "display_window",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "title", {"title", "window_id"});
              },
              {}, "0.4.0", "0.6.0"},
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "display_mouse",
      "Control the mouse cursor and pointer. action selects the operation: 'mode' sets the cursor behavior with mode 0=visible, 1=hidden, 2=captured (pointer locked to the window, used for FPS controls), 3=confined, 4=confined_hidden (values outside 0-4 return an error); 'warp' moves the cursor instantly to integer x, y in pixels relative to the client area of the focused window (not screen coordinates and not the basis reported by get_display_info action=mouse; the engine converts it onto the desktop) and is useful before injecting clicks with the input tools. Returns result 'ok'; behavior may vary slightly between platforms. Example: {\"action\": \"mode\", \"mode\": 2} or {\"action\": \"warp\", \"x\": 640, \"y\": 360}.",
      "Display", {"display", "mouse", "cursor", "warp"},
      SideEffect::ModifiesWindow, tool_flags::kMutating,
      kDisplayMouseParams, display_ops::handle_mouse_control,
      schema::build_action_schema(
          kDisplayMouseActions, kDisplayMouseParams,
          "mode: set the cursor behavior 0-4; warp: move the pointer to client-area coordinates x, y"),
      {},
      {"display_mouse action=mode mode=2",
       "display_mouse action=warp x=640 y=360"}},
      {
          ToolAlias{
              "set_display_mouse_mode", "display_mouse",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "mode", {"mode"});
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "warp_display_mouse", "display_mouse",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "warp", {"x", "y"});
              },
              {}, "0.4.0", "0.6.0"},
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "display_tts",
      "Speak text aloud through the system text-to-speech engine. action selects the operation: 'speak' requires text and optionally takes a voice id from get_display_info action=voices, volume 0-100 (default 50), pitch (default 1.0) and rate (default 1.0); speech is asynchronous. 'stop' interrupts any ongoing playback immediately and returns result 'ok' even when nothing is being spoken. 'pause' pauses the current utterance and 'resume' continues it (both return result 'ok'). 'is_speaking' returns {speaking} as a read-only probe usable before waiting on speech completion. Example: {\"action\": \"speak\", \"text\": \"Level complete\"} or {\"action\": \"is_speaking\"}.",
      "Display", {"display", "tts", "speech", "audio", "alert", "speaking"},
      SideEffect::ShowsAlert, tool_flags::kMutating,
      kDisplayTtsParams, display_ops::handle_tts_control,
      schema::build_action_schema(
          kDisplayTtsActions, kDisplayTtsParams,
          "speak: synthesize and speak text with optional voice, volume, pitch and rate; stop: interrupt ongoing speech; pause/resume: pause or continue the current utterance; is_speaking: report whether speech is active"),
      {},
      {"display_tts action=speak text=Level complete",
       "display_tts action=is_speaking",
       "display_tts action=stop"}},
      {
          ToolAlias{
              "speak_display_tts", "display_tts",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "speak",
                                   {"text", "voice", "volume", "pitch", "rate"});
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "stop_display_tts", "display_tts",
              [](const godot_autopilot::util::JsonValue &args) {
                return action_args(args, "stop", {});
              },
              {}, "0.4.0", "0.6.0"},
      })));
  v.push_back(make_spec_tool(ToolSpec{
      "delete_display_window",
      "Close and free a sub-window previously created with display_window action=create. Use it to clean up windows that are no longer needed. Takes window_id as returned by display_window action=create; errors when no window with the given id exists and refuses window_id 0, which is the editor main window and is never destroyed by this tool. The window is freed at the end of the frame. This tool is classified as a destructive side effect because it destroys an existing window; confirm=true is required. Example: {\"window_id\": 2, \"confirm\": true}.",
      "Display", {"display", "window", "delete", "destructive"},
      SideEffect::Destructive, tool_flags::kMutating,
      kDeleteDisplayWindowParams, display_ops::handle_window_delete,
      {}, {}, {"{\"window_id\": 2, \"confirm\": true}"}}));
  return v;
}

} // namespace display_tools
} // namespace godot_autopilot

#endif
