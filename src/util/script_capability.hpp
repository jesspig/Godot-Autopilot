#ifndef GODOT_AUTOPILOT_SCRIPT_CAPABILITY_HPP
#define GODOT_AUTOPILOT_SCRIPT_CAPABILITY_HPP

#include <string>

#include "util/json_value.hpp"

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include "util/error_util.hpp"

namespace godot_autopilot {
namespace script_capability {

enum class Code {
  kMountable,
  kRuntimeOnly,
  kClassMissing,
  kNotANode,
  kAbstract,
  kInvalidScript,
  kNotAScript,
  kNotLoadable,
};

struct Result {
  bool loadable = false;
  bool is_packed_scene = false;
  bool is_script = false;
  std::string resource_class;
  std::string base_class;
  bool is_node = false;
  bool is_abstract = false;
  bool is_tool = false;
  bool editor_runnable = false;
  bool runtime_mountable = false;
  Code code = Code::kNotLoadable;
};

inline const char *code_name(Code code) {
  switch (code) {
    case Code::kMountable:
      return "mountable";
    case Code::kRuntimeOnly:
      return "runtime_only";
    case Code::kClassMissing:
      return "script_class_not_found";
    case Code::kNotANode:
      return "base_type_not_node";
    case Code::kAbstract:
      return "abstract_script";
    case Code::kInvalidScript:
      return "invalid_script";
    case Code::kNotAScript:
      return "not_a_script";
    case Code::kNotLoadable:
      return "not_loadable";
  }
  return "not_loadable";
}

inline Result classify(bool loadable, bool is_packed_scene, bool is_script,
                       const std::string &resource_class,
                       const std::string &base_class, bool is_abstract,
                       bool is_tool, bool can_instantiate, bool base_is_node) {
  Result r;
  r.loadable = loadable;
  r.is_packed_scene = is_packed_scene;
  r.is_script = is_script;
  r.resource_class = resource_class;
  r.base_class = base_class;
  r.is_abstract = is_abstract;
  r.is_tool = is_tool;
  if (!loadable) {
    r.code = Code::kNotLoadable;
    return r;
  }
  if (is_packed_scene) {
    r.is_node = base_is_node;
    r.editor_runnable = can_instantiate && base_is_node;
    r.runtime_mountable = r.editor_runnable;
    r.code = base_is_node ? Code::kMountable : Code::kNotANode;
    return r;
  }
  if (!is_script) {
    r.code = Code::kNotAScript;
    return r;
  }
  if (base_class.empty()) {
    r.code = Code::kClassMissing;
    return r;
  }
  r.is_node = base_is_node;
  if (!base_is_node) {
    r.code = Code::kNotANode;
    return r;
  }
  if (is_abstract) {
    r.code = Code::kAbstract;
    return r;
  }
  if (can_instantiate) {
    r.editor_runnable = true;
    r.runtime_mountable = true;
    r.code = Code::kMountable;
    return r;
  }
  if (resource_class == "CSharpScript") {
    r.runtime_mountable = true;
    r.code = Code::kRuntimeOnly;
    return r;
  }
  r.code = Code::kInvalidScript;
  return r;
}

inline bool base_is_node(const std::string &base_class) {
  if (base_class.empty())
    return false;
  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (cdbs == nullptr)
    return false;
  return cdbs->is_parent_class(godot::StringName(base_class.c_str()),
                               godot::StringName("Node"));
}

inline Result inspect_loaded(const godot::Ref<godot::Resource> &res) {
  if (res.is_null())
    return classify(false, false, false, "", "", false, false, false, false);
  auto *packed = godot::Object::cast_to<godot::PackedScene>(res.ptr());
  if (packed != nullptr) {
    godot::Node *instance = packed->instantiate();
    bool ok = instance != nullptr;
    if (instance != nullptr)
      memdelete(instance);
    return classify(true, true, false, "PackedScene", ok ? "Node" : "", false,
                    false, packed->can_instantiate(), ok);
  }
  auto *script = godot::Object::cast_to<godot::Script>(res.ptr());
  if (script == nullptr)
    return classify(true, false, false, util::to_std(res->get_class()), "",
                    false, false, false, false);
  const std::string base = util::to_std(script->get_instance_base_type());
  return classify(true, false, true, util::to_std(res->get_class()), base,
                  script->is_abstract(), script->is_tool(),
                  script->can_instantiate(), base_is_node(base));
}

inline Result inspect(const std::string &res_path, bool fresh_load) {
  auto *loader = godot::ResourceLoader::get_singleton();
  if (loader == nullptr)
    return classify(false, false, false, "", "", false, false, false, false);
  const int64_t cache_mode =
      fresh_load ? static_cast<int64_t>(godot::ResourceLoader::CACHE_MODE_IGNORE)
                 : static_cast<int64_t>(godot::ResourceLoader::CACHE_MODE_REUSE);
  godot::Ref<godot::Resource> res = loader->load(
      godot::String::utf8(res_path.c_str()), godot::String(),
      static_cast<godot::ResourceLoader::CacheMode>(cache_mode));
  return inspect_loaded(res);
}

inline godot_autopilot::util::JsonValue detail_json(const Result &result,
                                  const std::string &path) {
  godot_autopilot::util::JsonValue detail(godot_autopilot::util::JsonValue::object_tag);
  detail["code"] = godot_autopilot::util::JsonValue(code_name(result.code));
  detail["path"] = godot_autopilot::util::JsonValue(path);
  detail["resource_class"] = godot_autopilot::util::JsonValue(result.resource_class);
  detail["base_class"] = godot_autopilot::util::JsonValue(result.base_class);
  detail["runtime_mountable"] = godot_autopilot::util::JsonValue(result.runtime_mountable);
  detail["editor_runnable"] = godot_autopilot::util::JsonValue(result.editor_runnable);
  return detail;
}

} // namespace script_capability
} // namespace godot_autopilot

#endif
