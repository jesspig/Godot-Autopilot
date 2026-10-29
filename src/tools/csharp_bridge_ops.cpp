#include "csharp_bridge_ops.hpp"

#include "core/config.hpp"
#include "core/log_system.hpp"
#include "core/resource_registry.hpp"
#include "runtime/gda_protocol.hpp"
#include "tools/authorization.hpp"
#include "tools/runtime_ops.hpp"
#include "tools/tool_base.hpp"
#include "util/error_util.hpp"
#include "util/scene_path.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace csharp_bridge_ops {

namespace {

using JV = mcp::JsonValue;

constexpr const char *kBridgeSingleton = "GdaCSharpBridge";
constexpr const char *kBridgeScriptPath =
    "res://addons/godot-autopilot/GdaCSharpBridge.cs";
constexpr const char *kBridgeHolderNode = "__gda_runtime";
constexpr size_t kMaxCallArgs = 16;
constexpr int64_t kMemberListDefaultLimit = 200;
constexpr int64_t kMemberListMaxLimit = 1000;

struct CSharpTarget {
  bool ok = false;
  godot::Object *object = nullptr;
  bool is_scene_path = false;
  std::string scene_path;
  JV error;
};

JV locator_error(const std::string &fact, const std::string &position) {
  return util::error_detail(
      fact, position,
      "exactly one of object_id, object_id_str, memory or path",
      "pass a live object_id, a memory resource name, a res:// resource path "
      "or a scene node path");
}

CSharpTarget resolve_locator(const JV &args, const std::string &prefix,
                             const std::string &tool) {
  CSharpTarget out;
  const std::string k_oid = prefix + "object_id";
  const std::string k_oid_str = prefix + "object_id_str";
  const std::string k_memory = prefix + "memory";
  const std::string k_path = prefix + "path";
  const JV *p_oid = args.Find(k_oid);
  const JV *p_oid_str = args.Find(k_oid_str);
  const JV *p_memory = args.Find(k_memory);
  const JV *p_path = args.Find(k_path);
  int present = (p_oid ? 1 : 0) + (p_oid_str ? 1 : 0) + (p_memory ? 1 : 0) +
                (p_path ? 1 : 0);
  if (present == 0) {
    out.error = util::error_json("missing required parameter: " + k_path +
                                 " (or " + k_oid + ", " + k_oid_str + ", " +
                                 k_memory + ")");
    return out;
  }
  if (present > 1) {
    out.error = util::error_json(
        k_oid + ", " + k_oid_str + ", " + k_memory + " and " + k_path +
        " are mutually exclusive for " + tool + " — pass exactly one");
    return out;
  }
  if (p_oid) {
    if (!p_oid->IsInt()) {
      out.error = util::error_json(k_oid + " must be an integer object id");
      return out;
    }
    godot::Object *obj = godot::ObjectDB::get_instance(
        static_cast<uint64_t>(p_oid->GetInt()));
    if (!obj) {
      out.error = locator_error(
          k_oid + " " + std::to_string(p_oid->GetInt()) +
              " does not reference a live object (freed or never existed)",
          tool);
      return out;
    }
    out.ok = true;
    out.object = obj;
    return out;
  }
  if (p_oid_str) {
    if (!p_oid_str->IsString() || p_oid_str->GetString().empty()) {
      out.error =
          util::error_json(k_oid_str + " must be a non-empty integer string");
      return out;
    }
    int64_t obj_id = 0;
    try {
      obj_id = std::stoll(p_oid_str->GetString());
    } catch (...) {
      out.error = util::error_json("invalid " + k_oid_str +
                                   ": not a valid 64-bit integer");
      return out;
    }
    godot::Object *obj =
        godot::ObjectDB::get_instance(static_cast<uint64_t>(obj_id));
    if (!obj) {
      out.error = locator_error(k_oid_str + " " + p_oid_str->GetString() +
                                    " does not reference a live object "
                                    "(freed or never existed)",
                                tool);
      return out;
    }
    out.ok = true;
    out.object = obj;
    return out;
  }
  if (p_memory) {
    if (!p_memory->IsString() || p_memory->GetString().empty()) {
      out.error =
          util::error_json(k_memory + " must be a non-empty resource name");
      return out;
    }
    godot::Ref<godot::Resource> res =
        resource_registry::lookup_memory(p_memory->GetString());
    if (res.is_null()) {
      out.error = locator_error("memory resource '" + p_memory->GetString() +
                                    "' not found (create it first, e.g. with "
                                    "create_resource)",
                                tool);
      return out;
    }
    out.ok = true;
    out.object = res.ptr();
    return out;
  }
  if (!p_path->IsString() || p_path->GetString().empty()) {
    out.error = util::error_json(k_path + " must be a non-empty string");
    return out;
  }
  const std::string path_str = p_path->GetString();
  const std::string res_prefix = "res://";
  const std::string memory_prefix = "memory://";
  if (path_str.compare(0, res_prefix.size(), res_prefix) == 0) {
    auto *loader = godot::ResourceLoader::get_singleton();
    godot::Ref<godot::Resource> res;
    if (loader) {
      res = loader->load(godot::String(path_str.c_str()));
    }
    if (res.is_null()) {
      out.error = locator_error("failed to load resource '" + path_str +
                                    "' from disk (missing file or loader "
                                    "unavailable)",
                                tool);
      return out;
    }
    if (res->is_class("Script")) {
      out.error = util::error_detail(
          "path '" + path_str + "' resolved to a Script resource, not a live object",
          k_path, "a scene node path or object_id_str of a live instance",
          "pass the node path (e.g. 'World/Player') or object_id_str; a .cs/.gd "
          "path addresses the script resource, not its instances");
      return out;
    }
    out.ok = true;
    out.object = res.ptr();
    return out;
  }
  if (path_str.compare(0, memory_prefix.size(), memory_prefix) == 0) {
    out.error = util::error_json(
        k_path + " '" + path_str +
        "' is a resource namespace, not a node path — pass the bare name in " +
        k_memory + " instead");
    return out;
  }
  out.ok = true;
  out.is_scene_path = true;
  out.scene_path = path_str;
  return out;
}

JV resolve_editor_object(CSharpTarget &target) {
  if (!target.is_scene_path)
    return JV();
  godot::Node *scene_root = nullptr;
  if (auto *editor = godot::EditorInterface::get_singleton()) {
    scene_root = editor->get_edited_scene_root();
  }
  std::string hint;
  godot::Node *node =
      util::resolve_scene_node(target.scene_path, scene_root, &hint);
  if (!node) {
    target.ok = false;
    target.error = util::error_json("node not found: " + target.scene_path +
                                    " — " + hint);
    return target.error;
  }
  target.object = node;
  return JV();
}

bool parse_runtime(const JV &args, const std::string &tool, bool &out_game,
                   JV &err) {
  out_game = false;
  const JV *p = args.Find("runtime");
  if (!p)
    return true;
  if (!p->IsString() || (p->GetString() != "editor" && p->GetString() != "game")) {
    err = util::error_json(
        "runtime must be \"editor\" or \"game\" for " + tool);
    return false;
  }
  out_game = (p->GetString() == "game");
  return true;
}

bool parse_game_options(const JV &args, const std::string &tool, bool is_game,
                        int64_t &timeout_ms, bool &wait_ready, JV &err) {
  timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
  wait_ready = true;
  const JV *tp = args.Find("timeout_ms");
  const JV *wp = args.Find("wait_ready");
  if (!is_game) {
    if (tp) {
      err = util::error_json("timeout_ms only applies when runtime=\"game\"");
      return false;
    }
    if (wp) {
      err = util::error_json("wait_ready only applies when runtime=\"game\"");
      return false;
    }
    return true;
  }
  if (tp) {
    if (!tp->IsInt() || tp->GetInt() <= 0 ||
        tp->GetInt() > GDA_MAX_GAME_OP_TIMEOUT_MS) {
      err = util::error_json("timeout_ms must be an integer between 1 and " +
                             std::to_string(GDA_MAX_GAME_OP_TIMEOUT_MS));
      return false;
    }
    timeout_ms = tp->GetInt();
  }
  if (wp) {
    if (!wp->IsBool()) {
      err = util::error_json("wait_ready must be a boolean");
      return false;
    }
    wait_ready = wp->GetBool();
  }
  return true;
}

bool check_allowed(const JV &args, const std::string &tool,
                   const char *const *allowed, size_t count, JV &err) {
  if (!args.IsObject()) {
    err = util::error_json(tool + " parameters must be an object");
    return false;
  }
  for (const auto &entry : args.GetObject()) {
    bool known = false;
    for (size_t i = 0; i < count; i++) {
      if (entry.first == allowed[i]) {
        known = true;
        break;
      }
    }
    if (!known) {
      err = util::error_json("unknown parameter for " + tool + ": " +
                             entry.first);
      return false;
    }
  }
  return true;
}

bool fetch_editor_bridge(godot::Object *&out_bridge,
                         godot::Variant &out_holder, JV &err) {
  out_bridge = nullptr;
  out_holder = godot::Variant();
  auto *engine = godot::Engine::get_singleton();
  if (!engine) {
    err = util::error_json("Engine singleton not available");
    return false;
  }
  if (engine->has_singleton(kBridgeSingleton)) {
    out_bridge = engine->get_singleton(kBridgeSingleton);
    if (!out_bridge) {
      err = util::error_json(
          "csharp bridge singleton 'GdaCSharpBridge' resolved to null");
      return false;
    }
    return true;
  }
  auto *loader = godot::ResourceLoader::get_singleton();
  godot::Ref<godot::Resource> script;
  if (loader) {
    script = loader->load(godot::String(kBridgeScriptPath));
  }
  if (script.is_null()) {
    err = util::error_json(
        "csharp bridge singleton 'GdaCSharpBridge' is not registered: run a "
        ".NET editor build with the bridge registered via "
        "Engine.register_singleton, then retry");
    return false;
  }
  godot::Variant instance = script->call(godot::StringName("new"));
  if (instance.get_type() == godot::Variant::NIL) {
    err = util::error_json(
        "csharp bridge singleton 'GdaCSharpBridge' is not registered: run a "
        ".NET editor build with the bridge registered via "
        "Engine.register_singleton, then retry");
    return false;
  }
  godot::Object *obj = instance;
  if (!obj) {
    err = util::error_json(
        "csharp bridge singleton 'GdaCSharpBridge' is not registered: run a "
        ".NET editor build with the bridge registered via "
        "Engine.register_singleton, then retry");
    return false;
  }
  out_holder = instance;
  out_bridge = obj;
  return true;
}

bool invoke_bridge(godot::Object *bridge, const std::string &method,
                   const godot::Array &bridge_args,
                   godot::Dictionary &out_result, JV &err) {
  godot::StringName sn(method.c_str());
  if (!bridge->has_method(sn)) {
    err = util::error_json("bridge method '" + method +
                           "' not found on GdaCSharpBridge (ABI mismatch: "
                           "expected ping/get_prop/set_prop/call_method/"
                           "subscribe/unsubscribe)");
    return false;
  }
  godot::Variant ret = bridge->callv(sn, bridge_args);
  if (ret.get_type() != godot::Variant::DICTIONARY) {
    err = util::error_json("bridge method '" + method +
                           "' did not return a Dictionary (ABI mismatch)");
    return false;
  }
  godot::Dictionary dict = ret;
  bool ok = false;
  if (dict.has("ok")) {
    godot::Variant ok_var = dict["ok"];
    if (ok_var.get_type() == godot::Variant::BOOL) {
      ok = ok_var.operator bool();
    }
  }
  if (!ok) {
    std::string message = "unknown bridge error";
    if (dict.has("error")) {
      godot::Variant err_var = dict["error"];
      if (err_var.get_type() == godot::Variant::STRING) {
        message = util::to_std(err_var.operator godot::String());
      } else if (err_var.get_type() == godot::Variant::STRING_NAME) {
        message = util::to_std(godot::String(err_var.operator godot::StringName()));
      }
    }
    err = util::error_json(message);
    return false;
  }
  out_result = dict;
  return true;
}

constexpr int64_t kBridgeAbiV1 = 1;
constexpr int64_t kBridgeAbiV2 = 2;

int64_t resolve_bridge_abi(godot::Object *bridge) {
  godot::Array empty;
  godot::Dictionary reply;
  JV probe_err;
  if (!invoke_bridge(bridge, "ping", empty, reply, probe_err)) {
    return kBridgeAbiV1;
  }
  if (!reply.has("abi")) {
    return kBridgeAbiV1;
  }
  godot::Variant abi_var = reply["abi"];
  if (abi_var.get_type() != godot::Variant::INT) {
    return kBridgeAbiV1;
  }
  int64_t abi = abi_var.operator int64_t();
  if (abi < kBridgeAbiV1) {
    return kBridgeAbiV1;
  }
  return abi;
}

bool invoke_bridge_abi(godot::Object *bridge, const std::string &method,
                       const godot::Array &bridge_args,
                       godot::Dictionary &out_result, JV &err) {
  int64_t abi = resolve_bridge_abi(bridge);
  if (abi < kBridgeAbiV2) {
    return invoke_bridge(bridge, method, bridge_args, out_result, err);
  }
  return invoke_bridge(bridge, method, bridge_args, out_result, err);
}

bool parse_call_args(const JV &args, godot::Array &out_args, JV &err) {
  const JV *p = args.Find("args");
  if (!p)
    return true;
  if (!p->IsArray()) {
    err = util::error_json("args must be a JSON array");
    return false;
  }
  if (p->GetArray().size() > kMaxCallArgs) {
    err = util::error_json("args exceeds maximum of " +
                           std::to_string(kMaxCallArgs) +
                           " entries — split into smaller calls");
    return false;
  }
  for (const auto &item : p->GetArray()) {
    out_args.append(VariantJson::deserialize(item));
  }
  return true;
}

std::string gdscript_escape(const std::string &text) {
  std::string out;
  for (char c : text) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  return out;
}

std::string bridge_repair_source() {
  std::string s;
  s += "var __gda_bridge_script = "
       "load(\"res://addons/godot-autopilot/GdaCSharpRuntime.cs\")\n";
  s += "if __gda_bridge_script == null:\n";
  s += "    return {\"ok\": false, \"error\": \"failed to load "
       "res://addons/godot-autopilot/GdaCSharpRuntime.cs; deploy the addon and "
       "build the .NET assembly first\"}\n";
  s += "var __gda_bridge_node = __gda_bridge_script.new()\n";
  s += "if __gda_bridge_node == null:\n";
  s += "    return {\"ok\": false, \"error\": \"failed to instantiate "
       "GdaCSharpRuntime; the running game assembly does not contain it yet — "
       "run build_csharp_assembly and restart the game\"}\n";
  s += "__gda_bridge_node.name = \"GdaCSharpRuntime\"\n";
  s += "get_tree().root.add_child(__gda_bridge_node)\n";
  s += "return {\"ok\": true, \"bridge_created\": true, \"bridge_ready\": "
       "get_tree().root.find_child(\"" +
       std::string(kBridgeHolderNode) + "\", true, false) != null}\n";
  return s;
}

std::string emit_bridge_missing_return(const std::string &detail) {
  std::string s;
  s += "return {\"ok\": false, \"error\": \"csharp bridge not found in the "
       "running game: " + detail + "\", \"suggested_action\": \"add the "
       "GdaCSharpRuntime bridge to the running game by executing the "
       "execute_game_script.source_code snippet (it loads "
       "res://addons/godot-autopilot/GdaCSharpRuntime.cs and adds it to the "
       "scene root), or add a GdaCSharpRuntime node to the game scene and "
       "relaunch\", \"execute_game_script\": {\"source_code\": \"" +
       gdscript_escape(bridge_repair_source()) + "\"}}\n";
  return s;
}

std::string emit_bridge_header() {
  std::string s;
  s += "var __gda_holder = get_tree().root.find_child(\"" +
       std::string(kBridgeHolderNode) + "\", true, false)\n";
  s += "if __gda_holder == null:\n";
  s += "    " + emit_bridge_missing_return(
      "no __gda_runtime node (add GdaCSharpRuntime to the game scene and run a "
      ".NET game build)");
  s += "if not __gda_holder.has_meta(\"bridge\"):\n";
  s += "    " + emit_bridge_missing_return(
      "__gda_runtime node has no bridge meta (GdaCSharpRuntime._Ready did not "
      "run)");
  s += "var __gda_bridge = __gda_holder.get_meta(\"bridge\")\n";
  return s;
}

std::string emit_resolve_node(const std::string &var,
                              const std::string &path) {
  const std::string esc = gdscript_escape(path);
  std::string s;
  s += "var " + var + " = get_tree().root.get_node_or_null(\"" + esc + "\")\n";
  s += "if " + var + " == null:\n";
  s += "    var __gda_scene = get_tree().current_scene\n";
  s += "    if __gda_scene != null:\n";
  s += "        " + var + " = __gda_scene.get_node_or_null(\"" + esc + "\")\n";
  s += "if " + var + " == null:\n";
  s += "    return {\"ok\": false, \"error\": \"node not found: " + esc +
       "\"}\n";
  return s;
}

JV forward_game_eval(const std::string &source, int64_t timeout_ms,
                     bool wait_ready) {
  JV params(JV::object_tag);
  params["action"] = JV("script");
  params["source_code"] = JV(source);
  return runtime_ops::handle_gda_send(
      std::string(GDA_OP_EVAL), params, timeout_ms,
      runtime_ops::needs_error_break_suppression(std::string(GDA_OP_EVAL),
                                                 "script"),
      wait_ready);
}

JV require_game_scene_path(const CSharpTarget &target,
                           const std::string &tool) {
  if (target.is_scene_path)
    return JV();
  return util::error_json(
      tool +
      " with runtime=\"game\" requires a scene node path locator (path); "
      "object_id, object_id_str and memory locators are editor-process "
      "handles and cannot cross into the running game");
}

constexpr const char *kGetAllowed[] = {
    "object_id", "object_id_str", "memory", "path", "property",
    "allow_non_public", "runtime", "timeout_ms", "wait_ready"};

constexpr const char *kCallAllowed[] = {
    "object_id", "object_id_str", "memory", "path", "method",
    "args",      "runtime",       "timeout_ms",    "wait_ready"};

constexpr const char *kSubscribeAllowed[] = {
    "object_id",        "object_id_str", "memory",         "path",
    "event",            "target_object_id", "target_object_id_str",
    "target_memory",    "target_path",   "method",         "runtime",
    "timeout_ms",       "wait_ready"};

constexpr const char *kSetAllowed[] = {
    "object_id", "object_id_str", "memory", "path", "property",
    "value",     "allow_non_public", "runtime",    "timeout_ms",
    "wait_ready"};

constexpr const char *kUnsubscribeAllowed[] = {
    "token", "runtime", "timeout_ms", "wait_ready"};

constexpr const char *kListAllowed[] = {
    "object_id", "object_id_str", "memory", "path",     "kind",
    "name_contains", "limit",    "offset", "runtime",  "timeout_ms",
    "wait_ready"};

std::string lowercase_ascii(std::string text) {
  for (char &c : text) {
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + ('a' - 'A'));
  }
  return text;
}

std::string csharp_member_name(const godot::Dictionary &item) {
  if (!item.has("name"))
    return "";
  return util::to_std(item["name"].operator godot::String());
}

bool csharp_kind_included(const std::string &kind, const char *table) {
  return kind == "all" || kind == table;
}

bool csharp_name_matches(const std::string &name_lower,
                         const std::string &needle_lower) {
  return needle_lower.empty() ||
         name_lower.find(needle_lower) != std::string::npos;
}

JV serialize_member_page(const std::vector<godot::Dictionary> &page) {
  JV arr(JV::array_tag);
  for (const godot::Dictionary &item : page)
    arr.PushBack(VariantJson::serialize(godot::Variant(item)));
  return arr;
}

JV build_csharp_member_page(godot::Object *obj, const std::string &kind,
                            const std::string &needle_lower, int64_t limit,
                            int64_t offset) {
  godot::TypedArray<godot::Dictionary> methods = obj->get_method_list();
  godot::TypedArray<godot::Dictionary> props = obj->get_property_list();
  godot::TypedArray<godot::Dictionary> signals = obj->get_signal_list();
  godot::TypedArray<godot::Dictionary> *tables[3] = {&methods, &props, &signals};
  const char *table_kinds[3] = {"method", "property", "signal"};
  std::vector<godot::Dictionary> filtered[3];
  for (int t = 0; t < 3; t++) {
    if (!csharp_kind_included(kind, table_kinds[t]))
      continue;
    const int64_t count = tables[t]->size();
    for (int64_t i = 0; i < count; i++) {
      godot::Dictionary item = (*tables[t])[i];
      if (csharp_name_matches(
              lowercase_ascii(csharp_member_name(item)), needle_lower))
        filtered[t].push_back(item);
    }
  }
  const int64_t counts[3] = {static_cast<int64_t>(filtered[0].size()),
                             static_cast<int64_t>(filtered[1].size()),
                             static_cast<int64_t>(filtered[2].size())};
  const int64_t total = counts[0] + counts[1] + counts[2];
  std::vector<godot::Dictionary> pages[3];
  int64_t cursor = 0;
  int64_t returned = 0;
  for (int t = 0; t < 3; t++) {
    for (const godot::Dictionary &item : filtered[t]) {
      if (cursor >= offset && returned < limit) {
        pages[t].push_back(item);
        ++returned;
      }
      ++cursor;
    }
  }
  JV counts_json(JV::object_tag);
  counts_json["methods"] = JV(counts[0]);
  counts_json["properties"] = JV(counts[1]);
  counts_json["signals"] = JV(counts[2]);
  counts_json["returned"] = JV(returned);
  counts_json["total"] = JV(total);
  const bool truncated = offset < total && total > offset + returned;
  JV members(JV::object_tag);
  members["methods"] = serialize_member_page(pages[0]);
  members["properties"] = serialize_member_page(pages[1]);
  members["signals"] = serialize_member_page(pages[2]);
  members["counts"] = std::move(counts_json);
  members["truncated"] = JV(truncated);
  return members;
}

std::string emit_list_filter_source(const std::string &kind,
                                    const std::string &needle_lower,
                                    int64_t limit, int64_t offset) {
  auto append_filter = [](std::string &s, const char *list_fn,
                          const char *out_var, const char *table_kind) {
    s += "var " + std::string(out_var) + " = []\n";
    s += "if __gda_kind == \"all\" or __gda_kind == \"" +
         std::string(table_kind) + "\":\n";
    s += "    for __gda_item in __gda_target." + std::string(list_fn) + "():\n";
    s += "        if __gda_needle == \"\" or "
         "String(__gda_item.get(\"name\", \"\")).to_lower().find("
         "__gda_needle) != -1:\n";
    s += "            " + std::string(out_var) + ".append(__gda_item)\n";
  };
  std::string s;
  s += "var __gda_kind = \"" + gdscript_escape(kind) + "\"\n";
  s += "var __gda_needle = \"" + gdscript_escape(needle_lower) + "\"\n";
  s += "var __gda_limit = " + std::to_string(limit) + "\n";
  s += "var __gda_offset = " + std::to_string(offset) + "\n";
  append_filter(s, "get_method_list", "__gda_methods", "method");
  append_filter(s, "get_property_list", "__gda_props", "property");
  append_filter(s, "get_signal_list", "__gda_signals", "signal");
  s += "var __gda_total = __gda_methods.size() + __gda_props.size() + "
       "__gda_signals.size()\n";
  s += "var __gda_cursor = 0\n";
  s += "var __gda_returned = 0\n";
  s += "var __gda_methods_page = []\n";
  s += "var __gda_props_page = []\n";
  s += "var __gda_signals_page = []\n";
  s += "for __gda_pair in [[__gda_methods, __gda_methods_page], "
       "[__gda_props, __gda_props_page], [__gda_signals, "
       "__gda_signals_page]]:\n";
  s += "    for __gda_item in __gda_pair[0]:\n";
  s += "        if __gda_cursor >= __gda_offset and __gda_returned < "
       "__gda_limit:\n";
  s += "            __gda_pair[1].append(__gda_item)\n";
  s += "            __gda_returned += 1\n";
  s += "        __gda_cursor += 1\n";
  s += "return {\"methods\": __gda_methods_page, \"properties\": "
       "__gda_props_page, \"signals\": __gda_signals_page, \"counts\": "
       "{\"methods\": __gda_methods.size(), \"properties\": "
       "__gda_props.size(), \"signals\": __gda_signals.size(), "
       "\"returned\": __gda_returned, \"total\": __gda_total}, "
       "\"truncated\": __gda_total > __gda_offset + __gda_returned}\n";
  return s;
}

} // namespace

mcp::JsonValue handle_csharp_get(const mcp::JsonValue &args) {
  const std::string tool = "csharp_get_property";
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "csharp_get_property called");
  JV err;
  if (!check_allowed(args, tool, kGetAllowed,
                     sizeof(kGetAllowed) / sizeof(*kGetAllowed), err)) {
    return err;
  }
  bool is_game = false;
  if (!parse_runtime(args, tool, is_game, err)) {
    return err;
  }
  int64_t timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
  bool wait_ready = true;
  if (!parse_game_options(args, tool, is_game, timeout_ms, wait_ready, err)) {
    return err;
  }
  const JV *prop_p = args.Find("property");
  if (!prop_p || !prop_p->IsString() || prop_p->GetString().empty()) {
    return util::error_json(
        "missing required parameter: property (non-empty C# member name)");
  }
  const std::string prop = prop_p->GetString();
  bool allow_non_public = false;
  if (const JV *allow_p = args.Find("allow_non_public")) {
    if (!allow_p->IsBool()) {
      return util::error_json("allow_non_public must be a boolean");
    }
    allow_non_public = allow_p->GetBool();
  }
  CSharpTarget target = resolve_locator(args, "", tool);
  if (!target.ok) {
    return target.error;
  }
  if (is_game) {
    JV denied = authorization::deny_if_unauthorized(
        "csharp_get_property", SideEffect::GameRuntime);
    if (!denied.IsNull()) {
      return denied;
    }
    JV locator_err = require_game_scene_path(target, tool);
    if (!locator_err.IsNull()) {
      return locator_err;
    }
    std::string source = emit_bridge_header();
    source += emit_resolve_node("__gda_target", target.scene_path);
    source += "return __gda_bridge.get_prop(__gda_target, &\"" +
              gdscript_escape(prop) + "\", " +
              (allow_non_public ? "true" : "false") + ")\n";
    return forward_game_eval(source, timeout_ms, wait_ready);
  }
  if (!resolve_editor_object(target).IsNull()) {
    return target.error;
  }
  godot::Object *bridge = nullptr;
  godot::Variant bridge_holder;
  if (!fetch_editor_bridge(bridge, bridge_holder, err)) {
    return err;
  }
  godot::Array bridge_args;
  bridge_args.append(godot::Variant(target.object));
  bridge_args.append(godot::Variant(godot::StringName(prop.c_str())));
  bridge_args.append(godot::Variant(allow_non_public));
  godot::Dictionary dict;
  if (!invoke_bridge_abi(bridge, "get_prop", bridge_args, dict, err)) {
    return err;
  }
  JV r(JV::object_tag);
  r["result"] = VariantJson::serialize(dict["value"]);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "csharp_get_property completed");
  return r;
}

mcp::JsonValue handle_csharp_set(const mcp::JsonValue &args) {
  const std::string tool = "csharp_set_property";
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "csharp_set_property called");
  JV err;
  if (!check_allowed(args, tool, kSetAllowed,
                     sizeof(kSetAllowed) / sizeof(*kSetAllowed), err)) {
    return err;
  }
  bool is_game = false;
  if (!parse_runtime(args, tool, is_game, err)) {
    return err;
  }
  int64_t timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
  bool wait_ready = true;
  if (!parse_game_options(args, tool, is_game, timeout_ms, wait_ready, err)) {
    return err;
  }
  const JV *prop_p = args.Find("property");
  if (!prop_p || !prop_p->IsString() || prop_p->GetString().empty()) {
    return util::error_json(
        "missing required parameter: property (non-empty C# member name)");
  }
  const std::string prop = prop_p->GetString();
  const JV *value_p = args.Find("value");
  if (!value_p) {
    return util::error_json("missing required parameter: value");
  }
  bool allow_non_public = false;
  if (const JV *allow_p = args.Find("allow_non_public")) {
    if (!allow_p->IsBool()) {
      return util::error_json("allow_non_public must be a boolean");
    }
    allow_non_public = allow_p->GetBool();
  }
  CSharpTarget target = resolve_locator(args, "", tool);
  if (!target.ok) {
    return target.error;
  }
  if (is_game) {
    JV denied = authorization::deny_if_unauthorized(
        "csharp_set_property", SideEffect::GameRuntime);
    if (!denied.IsNull()) {
      return denied;
    }
    JV locator_err = require_game_scene_path(target, tool);
    if (!locator_err.IsNull()) {
      return locator_err;
    }
    std::string source = emit_bridge_header();
    source += emit_resolve_node("__gda_target", target.scene_path);
    source += "var __gda_value = JSON.parse_string(\"" +
              gdscript_escape(value_p->Dump()) + "\")\n";
    source += "return __gda_bridge.set_prop(__gda_target, &\"" +
              gdscript_escape(prop) + "\", __gda_value, " +
              (allow_non_public ? "true" : "false") + ")\n";
    return forward_game_eval(source, timeout_ms, wait_ready);
  }
  if (!resolve_editor_object(target).IsNull()) {
    return target.error;
  }
  godot::Object *bridge = nullptr;
  godot::Variant bridge_holder;
  if (!fetch_editor_bridge(bridge, bridge_holder, err)) {
    return err;
  }
  godot::Array bridge_args;
  bridge_args.append(godot::Variant(target.object));
  bridge_args.append(godot::Variant(godot::StringName(prop.c_str())));
  bridge_args.append(VariantJson::deserialize(*value_p));
  bridge_args.append(godot::Variant(allow_non_public));
  godot::Dictionary dict;
  if (!invoke_bridge_abi(bridge, "set_prop", bridge_args, dict, err)) {
    return err;
  }
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "csharp_set_property completed");
  return r;
}

mcp::JsonValue handle_csharp_call(const mcp::JsonValue &args) {
  const std::string tool = "csharp_call_method";
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "csharp_call_method called");
  JV err;
  if (!check_allowed(args, tool, kCallAllowed,
                     sizeof(kCallAllowed) / sizeof(*kCallAllowed), err)) {
    return err;
  }
  bool is_game = false;
  if (!parse_runtime(args, tool, is_game, err)) {
    return err;
  }
  int64_t timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
  bool wait_ready = true;
  if (!parse_game_options(args, tool, is_game, timeout_ms, wait_ready, err)) {
    return err;
  }
  const JV *method_p = args.Find("method");
  if (!method_p || !method_p->IsString() || method_p->GetString().empty()) {
    return util::error_json(
        "missing required parameter: method (non-empty C# method name)");
  }
  const std::string method = method_p->GetString();
  godot::Array call_args;
  if (!parse_call_args(args, call_args, err)) {
    return err;
  }
  CSharpTarget target = resolve_locator(args, "", tool);
  if (!target.ok) {
    return target.error;
  }
  if (is_game) {
    JV denied = authorization::deny_if_unauthorized(
        "csharp_call_method", SideEffect::GameRuntime);
    if (!denied.IsNull()) {
      return denied;
    }
    JV locator_err = require_game_scene_path(target, tool);
    if (!locator_err.IsNull()) {
      return locator_err;
    }
    JV args_json(JV::array_tag);
    if (const JV *p = args.Find("args")) {
      args_json = *p;
    }
    std::string source = emit_bridge_header();
    source += emit_resolve_node("__gda_target", target.scene_path);
    source += "var __gda_parsed = JSON.parse_string(\"" +
              gdscript_escape(args_json.Dump()) + "\")\n";
    source += "if typeof(__gda_parsed) != TYPE_ARRAY:\n";
    source += "    return {\"ok\": false, \"error\": \"csharp_call_method: "
              "failed to decode args JSON in the running game\"}\n";
    source += "return __gda_bridge.call_method(__gda_target, &\"" +
              gdscript_escape(method) + "\", __gda_parsed)\n";
    return forward_game_eval(source, timeout_ms, wait_ready);
  }
  if (!resolve_editor_object(target).IsNull()) {
    return target.error;
  }
  godot::Object *bridge = nullptr;
  godot::Variant bridge_holder;
  if (!fetch_editor_bridge(bridge, bridge_holder, err)) {
    return err;
  }
  godot::Array bridge_args;
  bridge_args.append(godot::Variant(target.object));
  bridge_args.append(godot::Variant(godot::StringName(method.c_str())));
  bridge_args.append(godot::Variant(call_args));
  godot::Dictionary dict;
  if (!invoke_bridge_abi(bridge, "call_method", bridge_args, dict, err)) {
    return err;
  }
  JV r(JV::object_tag);
  r["result"] = VariantJson::serialize(dict["value"]);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "csharp_call_method completed");
  return r;
}

mcp::JsonValue handle_csharp_subscribe(const mcp::JsonValue &args) {
  const std::string tool = "csharp_subscribe_event";
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "csharp_subscribe_event called");
  JV err;
  if (!check_allowed(args, tool, kSubscribeAllowed,
                     sizeof(kSubscribeAllowed) / sizeof(*kSubscribeAllowed),
                     err)) {
    return err;
  }
  bool is_game = false;
  if (!parse_runtime(args, tool, is_game, err)) {
    return err;
  }
  int64_t timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
  bool wait_ready = true;
  if (!parse_game_options(args, tool, is_game, timeout_ms, wait_ready, err)) {
    return err;
  }
  const JV *event_p = args.Find("event");
  if (!event_p || !event_p->IsString() || event_p->GetString().empty()) {
    return util::error_json(
        "missing required parameter: event (non-empty C# event name)");
  }
  const JV *method_p = args.Find("method");
  if (!method_p || !method_p->IsString() || method_p->GetString().empty()) {
    return util::error_json("missing required parameter: method (non-empty "
                            "receiver method name)");
  }
  const std::string event_name = event_p->GetString();
  const std::string method = method_p->GetString();
  CSharpTarget source = resolve_locator(args, "", tool);
  if (!source.ok) {
    return source.error;
  }
  CSharpTarget sink = resolve_locator(args, "target_", tool);
  if (!sink.ok) {
    return sink.error;
  }
  if (is_game) {
    JV denied = authorization::deny_if_unauthorized(
        "csharp_subscribe_event", SideEffect::GameRuntime);
    if (!denied.IsNull()) {
      return denied;
    }
    JV source_err = require_game_scene_path(source, tool);
    if (!source_err.IsNull()) {
      return source_err;
    }
    if (!sink.is_scene_path) {
      return util::error_json(
          tool + " with runtime=\"game\" requires a scene node path "
                 "receiver (target_path); target_object_id, "
                 "target_object_id_str and target_memory locators are "
                 "editor-process handles and cannot cross into the running "
                 "game");
    }
    std::string script = emit_bridge_header();
    script += emit_resolve_node("__gda_source", source.scene_path);
    script += emit_resolve_node("__gda_sink", sink.scene_path);
    script += "return __gda_bridge.subscribe(__gda_source, \"" +
              gdscript_escape(event_name) + "\", __gda_sink, &\"" +
              gdscript_escape(method) + "\")\n";
    return forward_game_eval(script, timeout_ms, wait_ready);
  }
  if (!resolve_editor_object(source).IsNull()) {
    return source.error;
  }
  if (!resolve_editor_object(sink).IsNull()) {
    return sink.error;
  }
  godot::Object *bridge = nullptr;
  godot::Variant bridge_holder;
  if (!fetch_editor_bridge(bridge, bridge_holder, err)) {
    return err;
  }
  godot::Array bridge_args;
  bridge_args.append(godot::Variant(source.object));
  bridge_args.append(godot::Variant(godot::String(event_name.c_str())));
  bridge_args.append(godot::Variant(sink.object));
  bridge_args.append(godot::Variant(godot::StringName(method.c_str())));
  godot::Dictionary dict;
  if (!invoke_bridge_abi(bridge, "subscribe", bridge_args, dict, err)) {
    return err;
  }
  if (!dict.has("token")) {
    return util::error_json(
        "bridge subscribe returned no token (ABI mismatch)");
  }
  godot::Variant token_var = dict["token"];
  if (token_var.get_type() != godot::Variant::INT) {
    return util::error_json(
        "bridge subscribe returned a non-integer token (ABI mismatch)");
  }
  JV token(JV::object_tag);
  token["token"] = JV(static_cast<int64_t>(token_var));
  JV r(JV::object_tag);
  r["result"] = std::move(token);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "csharp_subscribe_event completed");
  return r;
}

mcp::JsonValue handle_csharp_unsubscribe(const mcp::JsonValue &args) {
  const std::string tool = "csharp_unsubscribe_event";
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "csharp_unsubscribe_event called");
  JV err;
  if (!check_allowed(args, tool, kUnsubscribeAllowed,
                     sizeof(kUnsubscribeAllowed) / sizeof(*kUnsubscribeAllowed),
                     err)) {
    return err;
  }
  bool is_game = false;
  if (!parse_runtime(args, tool, is_game, err)) {
    return err;
  }
  int64_t timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
  bool wait_ready = true;
  if (!parse_game_options(args, tool, is_game, timeout_ms, wait_ready, err)) {
    return err;
  }
  const JV *token_p = args.Find("token");
  if (!token_p) {
    return util::error_json("missing required parameter: token (integer "
                            "subscription token from csharp_subscribe_event)");
  }
  if (!token_p->IsInt()) {
    return util::error_json(
        "token must be an integer subscription token from "
        "csharp_subscribe_event");
  }
  const int64_t token = token_p->GetInt();
  if (token <= 0) {
    return util::error_json(
        "token must be a positive integer subscription token");
  }
  if (is_game) {
    JV denied = authorization::deny_if_unauthorized(
        "csharp_unsubscribe_event", SideEffect::GameRuntime);
    if (!denied.IsNull()) {
      return denied;
    }
    std::string source = emit_bridge_header();
    source += "return __gda_bridge.unsubscribe(" + std::to_string(token) +
              ")\n";
    return forward_game_eval(source, timeout_ms, wait_ready);
  }
  godot::Object *bridge = nullptr;
  godot::Variant bridge_holder;
  if (!fetch_editor_bridge(bridge, bridge_holder, err)) {
    return err;
  }
  godot::Array bridge_args;
  bridge_args.append(godot::Variant(token));
  godot::Dictionary dict;
  if (!invoke_bridge_abi(bridge, "unsubscribe", bridge_args, dict, err)) {
    return err;
  }
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "csharp_unsubscribe_event completed");
  return r;
}

mcp::JsonValue handle_csharp_list(const mcp::JsonValue &args) {
  const std::string tool = "csharp_list_members";
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "csharp_list_members called");
  JV err;
  if (!check_allowed(args, tool, kListAllowed,
                     sizeof(kListAllowed) / sizeof(*kListAllowed), err)) {
    return err;
  }
  bool is_game = false;
  if (!parse_runtime(args, tool, is_game, err)) {
    return err;
  }
  int64_t timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
  bool wait_ready = true;
  if (!parse_game_options(args, tool, is_game, timeout_ms, wait_ready, err)) {
    return err;
  }
  std::string kind = "all";
  if (const JV *kind_p = args.Find("kind")) {
    if (!kind_p->IsString()) {
      return util::error_json(
          "kind must be a string (method|property|signal|all)");
    }
    kind = kind_p->GetString();
    if (kind != "method" && kind != "property" && kind != "signal" &&
        kind != "all") {
      return util::error_json("kind must be method|property|signal|all");
    }
  }
  std::string name_contains;
  if (const JV *name_p = args.Find("name_contains")) {
    if (!name_p->IsString() || name_p->GetString().empty()) {
      return util::error_json("name_contains must be a non-empty string");
    }
    name_contains = lowercase_ascii(name_p->GetString());
  }
  int64_t limit = kMemberListDefaultLimit;
  if (const JV *limit_p = args.Find("limit")) {
    if (!limit_p->IsInt() || limit_p->GetInt() < 1 ||
        limit_p->GetInt() > kMemberListMaxLimit) {
      return util::error_json("limit must be an integer between 1 and " +
                              std::to_string(kMemberListMaxLimit));
    }
    limit = limit_p->GetInt();
  }
  int64_t offset = 0;
  if (const JV *offset_p = args.Find("offset")) {
    if (!offset_p->IsInt() || offset_p->GetInt() < 0) {
      return util::error_json("offset must be a non-negative integer");
    }
    offset = offset_p->GetInt();
  }
  CSharpTarget target = resolve_locator(args, "", tool);
  if (!target.ok) {
    return target.error;
  }
  if (is_game) {
    JV denied = authorization::deny_if_unauthorized(
        "csharp_list_members", SideEffect::GameRuntime);
    if (!denied.IsNull()) {
      return denied;
    }
    JV locator_err = require_game_scene_path(target, tool);
    if (!locator_err.IsNull()) {
      return locator_err;
    }
    std::string source = emit_resolve_node("__gda_target", target.scene_path);
    source += emit_list_filter_source(kind, name_contains, limit, offset);
    return forward_game_eval(source, timeout_ms, wait_ready);
  }
  if (!resolve_editor_object(target).IsNull()) {
    return target.error;
  }
  godot::Object *obj = target.object;
  if (!obj) {
    return util::error_json("node not found: " + target.scene_path);
  }
  JV lr(JV::object_tag);
  lr["result"] = build_csharp_member_page(obj, kind, name_contains, limit,
                                          offset);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "csharp_list_members completed");
  return lr;
}

} // namespace csharp_bridge_ops
} // namespace godot_autopilot
