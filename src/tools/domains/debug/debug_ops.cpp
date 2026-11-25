#include "tools/domains/debug/debug_ops.hpp"
#include "core/observability/log_system.hpp"
#include "util/error_util.hpp"
#include "util/input_guards.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_settings.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/performance.hpp>
#include <godot_cpp/classes/script_backtrace.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <string>

namespace godot_autopilot {
namespace debug_ops {

namespace {

struct MonitorInfo {
  const char *name;
  const char *type;
};

constexpr int64_t kMonitorMemoryStatic = 4;
constexpr int64_t kMonitorObjectCount = 7;
constexpr int64_t kMonitorNodeCount = 9;

static const MonitorInfo s_monitors[] = {
    {"time/fps", "time"},
    {"time/process", "time"},
    {"time/physics_process", "time"},
    {"time/navigation_process", "time"},
    {"memory/static", "memory"},
    {"memory/static_max", "memory"},
    {"memory/message_buffer_max", "memory"},
    {"object/count", "quantity"},
    {"object/resource_count", "quantity"},
    {"object/node_count", "quantity"},
    {"object/orphan_node_count", "quantity"},
    {"render/total_objects_in_frame", "quantity"},
    {"render/total_primitives_in_frame", "quantity"},
    {"render/total_draw_calls_in_frame", "quantity"},
    {"render/video_mem_used", "memory"},
    {"render/texture_mem_used", "memory"},
    {"render/buffer_mem_used", "memory"},
    {"physics/2d/active_objects", "quantity"},
    {"physics/2d/collision_pairs", "quantity"},
    {"physics/2d/island_count", "quantity"},
    {"physics/3d/active_objects", "quantity"},
    {"physics/3d/collision_pairs", "quantity"},
    {"physics/3d/island_count", "quantity"},
    {"audio/output_latency", "time"},
    {"navigation/active_maps", "quantity"},
    {"navigation/region_count", "quantity"},
    {"navigation/agent_count", "quantity"},
    {"navigation/link_count", "quantity"},
    {"navigation/polygon_count", "quantity"},
    {"navigation/edge_count", "quantity"},
    {"navigation/edge_merge_count", "quantity"},
    {"navigation/edge_connection_count", "quantity"},
    {"navigation/edge_free_count", "quantity"},
    {"navigation/obstacle_count", "quantity"},
    {"pipeline/compilations_canvas", "quantity"},
    {"pipeline/compilations_mesh", "quantity"},
    {"pipeline/compilations_surface", "quantity"},
    {"pipeline/compilations_draw", "quantity"},
    {"pipeline/compilations_specialization", "quantity"},
    {"navigation/2d/active_maps", "quantity"},
    {"navigation/2d/region_count", "quantity"},
    {"navigation/2d/agent_count", "quantity"},
    {"navigation/2d/link_count", "quantity"},
    {"navigation/2d/polygon_count", "quantity"},
    {"navigation/2d/edge_count", "quantity"},
    {"navigation/2d/edge_merge_count", "quantity"},
    {"navigation/2d/edge_connection_count", "quantity"},
    {"navigation/2d/edge_free_count", "quantity"},
    {"navigation/2d/obstacle_count", "quantity"},
    {"navigation/3d/active_maps", "quantity"},
    {"navigation/3d/region_count", "quantity"},
    {"navigation/3d/agent_count", "quantity"},
    {"navigation/3d/link_count", "quantity"},
    {"navigation/3d/polygon_count", "quantity"},
    {"navigation/3d/edge_count", "quantity"},
    {"navigation/3d/edge_merge_count", "quantity"},
    {"navigation/3d/edge_connection_count", "quantity"},
    {"navigation/3d/edge_free_count", "quantity"},
    {"navigation/3d/obstacle_count", "quantity"},
};

} // namespace

godot_autopilot::util::JsonValue handle_print(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "print_debug_log called");
  auto *mp = args.Find("message");
  if (!mp || !mp->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: message");
    return e;
  }
  std::string msg = mp->GetString();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, msg);
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "print_debug_log completed");
  return r;
}

godot_autopilot::util::JsonValue handle_print_stack(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debug_stack called");
  auto *ivp = args.Find("include_variables");
  bool include_vars = ivp && ivp->IsBool() ? ivp->GetBool() : false;
  auto *engine = godot::Engine::get_singleton();
  if (!engine) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Engine not available");
    return e;
  }
  auto traces = engine->capture_script_backtraces(include_vars);
  godot_autopilot::util::JsonValue result_arr(godot_autopilot::util::JsonValue::array_tag);
  for (int i = 0; i < traces.size(); i++) {
    auto *bt = godot::Object::cast_to<godot::ScriptBacktrace>(traces[i]);
    if (!bt)
      continue;
    godot_autopilot::util::JsonValue entry(godot_autopilot::util::JsonValue::object_tag);
    entry["language"] = godot_autopilot::util::JsonValue(util::to_std(bt->get_language_name()));
    int frame_count = bt->get_frame_count();
    entry["frame_count"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(frame_count));
    godot_autopilot::util::JsonValue frames(godot_autopilot::util::JsonValue::array_tag);
    for (int f = 0; f < frame_count; f++) {
      godot_autopilot::util::JsonValue frame(godot_autopilot::util::JsonValue::object_tag);
      frame["function"] = godot_autopilot::util::JsonValue(util::to_std(bt->get_frame_function(f)));
      frame["file"] = godot_autopilot::util::JsonValue(util::to_std(bt->get_frame_file(f)));
      frame["line"] =
          godot_autopilot::util::JsonValue(static_cast<int64_t>(bt->get_frame_line(f)));
      frames.PushBack(std::move(frame));
    }
    entry["frames"] = std::move(frames);
    if (include_vars) {
      int gvc = bt->get_global_variable_count();
      godot_autopilot::util::JsonValue globals(godot_autopilot::util::JsonValue::array_tag);
      for (int g = 0; g < gvc; g++) {
        godot_autopilot::util::JsonValue gv(godot_autopilot::util::JsonValue::object_tag);
        gv["name"] = godot_autopilot::util::JsonValue(util::to_std(bt->get_global_variable_name(g)));
        globals.PushBack(std::move(gv));
      }
      entry["global_variables"] = std::move(globals);
      godot_autopilot::util::JsonValue locals_arr(godot_autopilot::util::JsonValue::array_tag);
      for (int f = 0; f < frame_count; f++) {
        int lvc = bt->get_local_variable_count(f);
        godot_autopilot::util::JsonValue frame_locals(godot_autopilot::util::JsonValue::array_tag);
        for (int l = 0; l < lvc; l++) {
          godot_autopilot::util::JsonValue lv(godot_autopilot::util::JsonValue::object_tag);
          lv["name"] =
              godot_autopilot::util::JsonValue(util::to_std(bt->get_local_variable_name(f, l)));
          frame_locals.PushBack(std::move(lv));
        }
        locals_arr.PushBack(std::move(frame_locals));
      }
      entry["local_variables"] = std::move(locals_arr);
      godot_autopilot::util::JsonValue members_arr(godot_autopilot::util::JsonValue::array_tag);
      for (int f = 0; f < frame_count; f++) {
        int mvc = bt->get_member_variable_count(f);
        godot_autopilot::util::JsonValue frame_members(godot_autopilot::util::JsonValue::array_tag);
        for (int m = 0; m < mvc; m++) {
          godot_autopilot::util::JsonValue mv(godot_autopilot::util::JsonValue::object_tag);
          mv["name"] =
              godot_autopilot::util::JsonValue(util::to_std(bt->get_member_variable_name(f, m)));
          frame_members.PushBack(std::move(mv));
        }
        members_arr.PushBack(std::move(frame_members));
      }
      entry["member_variables"] = std::move(members_arr);
    }
    result_arr.PushBack(std::move(entry));
  }
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(result_arr);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debug_stack completed");
  return r;
}

godot_autopilot::util::JsonValue handle_set_physics_fps(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_debug_physics_fps called");
  auto *fp = args.Find("fps");
  if (!fp || !fp->IsNumber()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: fps (integer)");
    return e;
  }
  int64_t fps =
      fp->IsDouble() ? static_cast<int64_t>(fp->GetDouble()) : fp->GetInt();
  if (!util::physics_fps_is_valid(fps)) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("invalid parameter: fps must be between " +
                                std::to_string(util::kMinPhysicsFps) + " and " +
                                std::to_string(util::kMaxPhysicsFps));
    return e;
  }
  auto *engine = godot::Engine::get_singleton();
  if (!engine) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Engine not available");
    return e;
  }
  engine->set_physics_ticks_per_second(static_cast<int32_t>(fps));
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_debug_physics_fps completed");
  return r;
}

godot_autopilot::util::JsonValue handle_collision_debug(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_debug_collision_visual called");
  auto *ep = args.Find("enabled");
  if (!ep || !ep->IsBool()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: enabled (bool)");
    return e;
  }
  bool enabled = ep->GetBool();
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto es = editor->get_editor_settings();
  if (es.is_null()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorSettings not available");
    return e;
  }
  es->set_project_metadata("debug_options", "run_debug_collisions",
                           godot::Variant(enabled));
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_debug_collision_visual completed");
  return r;
}

godot_autopilot::util::JsonValue handle_navigation_debug(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_debug_navigation_visual called");
  auto *ep = args.Find("enabled");
  if (!ep || !ep->IsBool()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: enabled (bool)");
    return e;
  }
  bool enabled = ep->GetBool();
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto es = editor->get_editor_settings();
  if (es.is_null()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorSettings not available");
    return e;
  }
  es->set_project_metadata("debug_options", "run_debug_navigation",
                           godot::Variant(enabled));
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_debug_navigation_visual completed");
  return r;
}

godot_autopilot::util::JsonValue handle_performance_debug(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_debug_performance_visual called");
  auto *ep = args.Find("enabled");
  if (!ep || !ep->IsBool()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: enabled (bool)");
    return e;
  }
  bool enabled = ep->GetBool();
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto es = editor->get_editor_settings();
  if (es.is_null()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorSettings not available");
    return e;
  }
  es->set_project_metadata("debug_options", "run_debug_performance",
                           godot::Variant(enabled));
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_debug_performance_visual completed");
  return r;
}

godot_autopilot::util::JsonValue handle_get_all_monitors(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debug_monitors called");
  auto *perf = godot::Performance::get_singleton();
  if (!perf) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Performance singleton not available");
    return e;
  }
  const int count =
      static_cast<int>(sizeof(s_monitors) / sizeof(s_monitors[0]));
  const godot_autopilot::util::JsonValue *catalog_p = args.Find("catalog");
  if (catalog_p != nullptr && !catalog_p->IsBool()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] =
        godot_autopilot::util::JsonValue("invalid parameter: catalog must be a boolean");
    return e;
  }
  if (catalog_p != nullptr && catalog_p->GetBool()) {
    godot_autopilot::util::JsonValue arr(godot_autopilot::util::JsonValue::array_tag);
    for (int i = 0; i < count; i++) {
      godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
      item["id"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(i));
      item["name"] = godot_autopilot::util::JsonValue(s_monitors[i].name);
      item["type"] = godot_autopilot::util::JsonValue(s_monitors[i].type);
      arr.PushBack(std::move(item));
    }
    godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
    r["result"] = std::move(arr);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "get_debug_monitors completed");
    return r;
  }
  const godot_autopilot::util::JsonValue *ids_p = args.Find("ids");
  if (ids_p != nullptr) {
    if (!ids_p->IsArray()) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue(
          "invalid parameter: ids must be an array of integers");
      return e;
    }
    godot_autopilot::util::JsonValue arr(godot_autopilot::util::JsonValue::array_tag);
    for (const auto &value : ids_p->GetArray()) {
      int64_t monitor_id = 0;
      if (value.IsInt()) {
        monitor_id = value.GetInt();
      } else if (value.IsDouble()) {
        monitor_id = static_cast<int64_t>(value.GetDouble());
      } else {
        godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
        e["error"] = godot_autopilot::util::JsonValue(
            "invalid parameter: ids entries must be integers");
        return e;
      }
      if (monitor_id < 0 || monitor_id >= count) {
        godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
        e["error"] = godot_autopilot::util::JsonValue("monitor id out of range: " +
                                    std::to_string(monitor_id));
        return e;
      }
      godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
      item["id"] = godot_autopilot::util::JsonValue(monitor_id);
      item["name"] = godot_autopilot::util::JsonValue(s_monitors[monitor_id].name);
      item["type"] = godot_autopilot::util::JsonValue(s_monitors[monitor_id].type);
      item["value"] = godot_autopilot::util::JsonValue(perf->get_monitor(
          static_cast<godot::Performance::Monitor>(monitor_id)));
      arr.PushBack(std::move(item));
    }
    godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
    r["result"] = std::move(arr);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "get_debug_monitors completed");
    return r;
  }
  bool include_custom = false;
  if (const godot_autopilot::util::JsonValue *custom_p = args.Find("include_custom")) {
    if (!custom_p->IsBool()) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue(
          "invalid parameter: include_custom must be a boolean");
      return e;
    }
    include_custom = custom_p->GetBool();
  }
  godot_autopilot::util::JsonValue monitors(godot_autopilot::util::JsonValue::array_tag);
  for (int i = 0; i < count; i++) {
    godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
    item["name"] = godot_autopilot::util::JsonValue(s_monitors[i].name);
    item["type"] = godot_autopilot::util::JsonValue(s_monitors[i].type);
    item["value"] = godot_autopilot::util::JsonValue(perf->get_monitor(
        static_cast<godot::Performance::Monitor>(i)));
    monitors.PushBack(std::move(item));
  }
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  if (include_custom) {
    godot_autopilot::util::JsonValue names(godot_autopilot::util::JsonValue::array_tag);
    auto custom_names = perf->get_custom_monitor_names();
    for (int i = 0; i < custom_names.size(); i++) {
      names.PushBack(godot_autopilot::util::JsonValue(util::to_std(custom_names[i])));
    }
    godot_autopilot::util::JsonValue result_obj(godot_autopilot::util::JsonValue::object_tag);
    result_obj["monitors"] = std::move(monitors);
    result_obj["custom_names"] = std::move(names);
    r["result"] = std::move(result_obj);
  } else {
    r["result"] = std::move(monitors);
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debug_monitors completed");
  return r;
}

godot_autopilot::util::JsonValue handle_remove_custom_monitor(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "remove_debug_custom_monitor called");
  auto *id_p = args.Find("id");
  if (!id_p || !id_p->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: id");
    return e;
  }
  auto *perf = godot::Performance::get_singleton();
  if (!perf) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Performance singleton not available");
    return e;
  }
  perf->remove_custom_monitor(godot::StringName(id_p->GetString().c_str()));
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "remove_debug_custom_monitor completed");
  return r;
}

godot_autopilot::util::JsonValue handle_get_custom_monitor(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debug_custom_monitor called");
  auto *id_p = args.Find("id");
  if (!id_p || !id_p->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: id");
    return e;
  }
  auto *perf = godot::Performance::get_singleton();
  if (!perf) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Performance singleton not available");
    return e;
  }
  auto val =
      perf->get_custom_monitor(godot::StringName(id_p->GetString().c_str()));
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = VariantJson::serialize(val);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debug_custom_monitor completed");
  return r;
}

godot_autopilot::util::JsonValue handle_get_info(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debug_info called");
  auto *ap = args.Find("action");
  if (!ap || !ap->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: action");
    return e;
  }
  const std::string action = ap->GetString();
  if (action == "monitors") {
    return handle_get_all_monitors(args);
  }
  if (action == "catalog") {
    godot_autopilot::util::JsonValue call_args(godot_autopilot::util::JsonValue::object_tag);
    call_args["catalog"] = godot_autopilot::util::JsonValue(true);
    return handle_get_all_monitors(call_args);
  }
  if (action == "custom") {
    const godot_autopilot::util::JsonValue *id = args.Find("id");
    if (id != nullptr && id->IsString()) {
      return handle_get_custom_monitor(args);
    }
    godot_autopilot::util::JsonValue call_args(godot_autopilot::util::JsonValue::object_tag);
    call_args["include_custom"] = godot_autopilot::util::JsonValue(true);
    return handle_get_all_monitors(call_args);
  }
  int64_t monitor_id = -1;
  if (action == "object") {
    monitor_id = kMonitorObjectCount;
  } else if (action == "node") {
    monitor_id = kMonitorNodeCount;
  } else if (action == "memory") {
    monitor_id = kMonitorMemoryStatic;
  } else {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue(
        "unknown action '" + action +
        "': expected monitors, catalog, custom, object, node or memory");
    return e;
  }
  godot_autopilot::util::JsonValue call_args(godot_autopilot::util::JsonValue::object_tag);
  godot_autopilot::util::JsonValue ids(godot_autopilot::util::JsonValue::array_tag);
  ids.PushBack(godot_autopilot::util::JsonValue(monitor_id));
  call_args["ids"] = std::move(ids);
  godot_autopilot::util::JsonValue monitors = handle_get_all_monitors(call_args);
  const godot_autopilot::util::JsonValue *arr = monitors.Find("result");
  if (arr != nullptr && arr->IsArray() && arr->Size() > 0) {
    const godot_autopilot::util::JsonValue *value = (*arr)[0].Find("value");
    if (value != nullptr) {
      godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
      r["result"] = *value;
      return r;
    }
  }
  return monitors;
}

godot_autopilot::util::JsonValue handle_set_visuals(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_debug_visuals called");
  auto *ap = args.Find("action");
  if (!ap || !ap->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: action");
    return e;
  }
  const std::string action = ap->GetString();
  if (action == "collision") {
    return handle_collision_debug(args);
  }
  if (action == "navigation") {
    return handle_navigation_debug(args);
  }
  if (action == "performance") {
    return handle_performance_debug(args);
  }
  godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
  e["error"] = godot_autopilot::util::JsonValue(
      "unknown action '" + action +
      "': expected collision, navigation or performance");
  return e;
}

} // namespace debug_ops
} // namespace godot_autopilot
