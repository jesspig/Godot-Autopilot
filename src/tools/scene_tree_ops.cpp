#include "scene_tree_ops.hpp"
#include "core/config.hpp"
#include "core/log_system.hpp"
#include "runtime/gda_protocol.hpp"
#include "tools/runtime_ops.hpp"
#include <string>

namespace godot_autopilot {
namespace scene_tree_ops {

using JV = mcp::JsonValue;

JV handle_call_group(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "call_scene_tree_group called");
  auto *gn = args.Find("group_name");
  if (!gn || !gn->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: group_name");
    return e;
  }
  auto *mn = args.Find("method");
  if (!mn || !mn->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: method");
    return e;
  }
  JV params(JV::object_tag);
  params["action"] = JV("call_group");
  params["group_name"] = JV(gn->GetString());
  params["method"] = JV(mn->GetString());
  auto *ap = args.Find("arguments");
  if (ap && ap->IsArray()) {
    params["arguments"] = *ap;
  }
  return runtime_ops::handle_gda_send(std::string(GDA_OP_SCENE_TREE), params,
                                      GDA_DEFAULT_TIMEOUT_MS);
}

JV handle_create_timer(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_scene_tree_timer called");
  auto *dp = args.Find("delay_sec");
  if (!dp || !dp->IsNumber()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: delay_sec");
    return e;
  }
  JV params(JV::object_tag);
  params["action"] = JV("create_timer");
  if (dp->IsDouble()) {
    params["delay_sec"] = JV(dp->GetDouble());
  } else {
    params["delay_sec"] = JV(static_cast<double>(dp->GetInt()));
  }
  auto *pa = args.Find("process_always");
  if (pa && pa->IsBool()) {
    params["process_always"] = JV(pa->GetBool());
  }
  auto *pp = args.Find("process_in_physics");
  if (pp && pp->IsBool()) {
    params["process_in_physics"] = JV(pp->GetBool());
  }
  return runtime_ops::handle_gda_send(std::string(GDA_OP_SCENE_TREE), params,
                                      GDA_DEFAULT_TIMEOUT_MS);
}

JV handle_get_nodes_in_group(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_scene_tree_nodes_in_group called");
  auto *gn = args.Find("group_name");
  if (!gn || !gn->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: group_name");
    return e;
  }
  JV params(JV::object_tag);
  params["action"] = JV("get_nodes_in_group");
  params["group_name"] = JV(gn->GetString());
  return runtime_ops::handle_gda_send(std::string(GDA_OP_SCENE_TREE), params,
                                      GDA_DEFAULT_TIMEOUT_MS);
}

JV handle_is_paused(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "is_scene_tree_paused called");
  JV params(JV::object_tag);
  params["action"] = JV("is_paused");
  return runtime_ops::handle_gda_send(std::string(GDA_OP_SCENE_TREE), params,
                                      GDA_DEFAULT_TIMEOUT_MS);
}

JV handle_notify_group(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "notify_scene_tree_group called");
  auto *gn = args.Find("group_name");
  if (!gn || !gn->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: group_name");
    return e;
  }
  auto *ni = args.Find("notification");
  if (!ni || !ni->IsInt()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: notification");
    return e;
  }
  JV params(JV::object_tag);
  params["action"] = JV("notify_group");
  params["group_name"] = JV(gn->GetString());
  params["notification"] = JV(ni->GetInt());
  return runtime_ops::handle_gda_send(std::string(GDA_OP_SCENE_TREE), params,
                                      GDA_DEFAULT_TIMEOUT_MS);
}

JV handle_reload_current_scene(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "reload_scene_tree_current_scene called");
  JV params(JV::object_tag);
  params["action"] = JV("reload_current_scene");
  return runtime_ops::handle_gda_send(std::string(GDA_OP_SCENE_TREE), params,
                                      GDA_DEFAULT_TIMEOUT_MS);
}

JV handle_set_debug_collisions(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_scene_tree_debug_collisions_hint called");
  auto *ep = args.Find("enabled");
  if (!ep || !ep->IsBool()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: enabled");
    return e;
  }
  JV params(JV::object_tag);
  params["action"] = JV("set_debug_collisions_hint");
  params["enabled"] = JV(ep->GetBool());
  return runtime_ops::handle_gda_send(std::string(GDA_OP_SCENE_TREE), params,
                                      GDA_DEFAULT_TIMEOUT_MS);
}

JV handle_set_pause(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_scene_tree_pause called");
  auto *pp = args.Find("paused");
  if (!pp || !pp->IsBool()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: paused");
    return e;
  }
  JV params(JV::object_tag);
  params["action"] = JV("set_pause");
  params["paused"] = JV(pp->GetBool());
  return runtime_ops::handle_gda_send(std::string(GDA_OP_SCENE_TREE), params,
                                      GDA_DEFAULT_TIMEOUT_MS);
}

} // namespace scene_tree_ops
} // namespace godot_autopilot
