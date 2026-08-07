#include "doc_ops.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include <algorithm>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <string>

namespace godot_self_driving {
namespace doc_ops {

namespace {

mcp::JsonValue variant_to_json(const godot::Variant &v) {
  switch (v.get_type()) {
  case godot::Variant::NIL:
    return mcp::JsonValue(nullptr);
  case godot::Variant::BOOL:
    return mcp::JsonValue(static_cast<bool>(v));
  case godot::Variant::INT:
    return mcp::JsonValue(static_cast<int64_t>(v));
  case godot::Variant::FLOAT:
    return mcp::JsonValue(static_cast<double>(v));
  case godot::Variant::STRING: {
    godot::String s = static_cast<godot::String>(v);
    return mcp::JsonValue(util::to_std(s));
  }
  case godot::Variant::DICTIONARY: {
    mcp::JsonValue obj(mcp::JsonValue::object_tag);
    godot::Dictionary d = static_cast<godot::Dictionary>(v);
    godot::Array keys = d.keys();
    for (int i = 0; i < keys.size(); i++) {
      godot::String key = static_cast<godot::String>(keys[i]);
      obj[util::to_std(key)] = variant_to_json(d[keys[i]]);
    }
    return obj;
  }
  case godot::Variant::ARRAY:
  case godot::Variant::PACKED_STRING_ARRAY:
  case godot::Variant::PACKED_INT32_ARRAY:
  case godot::Variant::PACKED_FLOAT32_ARRAY:
  case godot::Variant::PACKED_INT64_ARRAY:
  case godot::Variant::PACKED_FLOAT64_ARRAY: {
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    godot::Array a = static_cast<godot::Array>(v);
    for (int i = 0; i < a.size(); i++) {
      arr.PushBack(variant_to_json(a[i]));
    }
    return arr;
  }
  default:
    return mcp::JsonValue(util::to_std(v.stringify()));
  }
}

} // namespace

mcp::JsonValue handle_get_class(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "documentation_get_class called");
  auto *np = args.Find("class");
  if (!np || !np->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: class");
    return e;
  }
  std::string class_name = np->GetString();
  auto *cd = godot::ClassDBSingleton::get_singleton();
  if (!cd) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ClassDBSingleton not available");
    return e;
  }
  godot::StringName sn(class_name.c_str());
  if (!cd->class_exists(sn)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("class not found: " + class_name);
    return e;
  }
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["name"] = mcp::JsonValue(class_name);
  result["parent_class"] =
      mcp::JsonValue(util::to_std(godot::String(cd->get_parent_class(sn))));
  result["api_type"] =
      mcp::JsonValue(static_cast<int64_t>(cd->class_get_api_type(sn)));
  result["can_instantiate"] = mcp::JsonValue(cd->can_instantiate(sn));
  {
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    godot::TypedArray<godot::Dictionary> methods =
        cd->class_get_method_list(sn, false);
    for (int i = 0; i < methods.size(); i++) {
      arr.PushBack(variant_to_json(methods[i]));
    }
    result["methods"] = std::move(arr);
  }
  {
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    godot::TypedArray<godot::Dictionary> props =
        cd->class_get_property_list(sn, false);
    for (int i = 0; i < props.size(); i++) {
      arr.PushBack(variant_to_json(props[i]));
    }
    result["properties"] = std::move(arr);
  }
  {
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    godot::TypedArray<godot::Dictionary> signals =
        cd->class_get_signal_list(sn, false);
    for (int i = 0; i < signals.size(); i++) {
      arr.PushBack(variant_to_json(signals[i]));
    }
    result["signals"] = std::move(arr);
  }
  {
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    godot::PackedStringArray enum_names = cd->class_get_enum_list(sn, false);
    for (int i = 0; i < enum_names.size(); i++) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      std::string ename = util::to_std(enum_names[i]);
      e["name"] = mcp::JsonValue(ename);
      mcp::JsonValue ec(mcp::JsonValue::array_tag);
      godot::PackedStringArray enum_consts =
          cd->class_get_enum_constants(sn, godot::StringName(ename.c_str()));
      for (int j = 0; j < enum_consts.size(); j++) {
        mcp::JsonValue cv(mcp::JsonValue::object_tag);
        std::string cname = util::to_std(enum_consts[j]);
        cv["name"] = mcp::JsonValue(cname);
        cv["value"] =
            mcp::JsonValue(static_cast<int64_t>(cd->class_get_integer_constant(
                sn, godot::StringName(cname.c_str()))));
        ec.PushBack(std::move(cv));
      }
      e["constants"] = std::move(ec);
      arr.PushBack(std::move(e));
    }
    result["enums"] = std::move(arr);
  }
  {
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    godot::PackedStringArray const_names =
        cd->class_get_integer_constant_list(sn, false);
    for (int i = 0; i < const_names.size(); i++) {
      mcp::JsonValue cv(mcp::JsonValue::object_tag);
      std::string cname = util::to_std(const_names[i]);
      cv["name"] = mcp::JsonValue(cname);
      cv["value"] =
          mcp::JsonValue(static_cast<int64_t>(cd->class_get_integer_constant(
              sn, godot::StringName(cname.c_str()))));
      arr.PushBack(std::move(cv));
    }
    result["constants"] = std::move(arr);
  }
  result["note"] = mcp::JsonValue("class reference without docstrings");
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(result);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "documentation_get_class completed");
  return r;
}

mcp::JsonValue handle_search(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "documentation_search called");
  auto *qp = args.Find("query");
  if (!qp || !qp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: query");
    return e;
  }
  std::string query = qp->GetString();
  auto *cd = godot::ClassDBSingleton::get_singleton();
  if (!cd) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ClassDBSingleton not available");
    return e;
  }
  std::string query_lower = query;
  std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                 ::tolower);
  godot::PackedStringArray all_classes = cd->get_class_list();
  mcp::JsonValue results_arr(mcp::JsonValue::array_tag);
  int count = 0;
  for (int i = 0; i < all_classes.size() && count < 50; i++) {
    std::string name = util::to_std(all_classes[i]);
    std::string name_lower = name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                   ::tolower);
    if (name_lower.find(query_lower) != std::string::npos) {
      mcp::JsonValue item(mcp::JsonValue::object_tag);
      item["name"] = mcp::JsonValue(name);
      item["parent"] = mcp::JsonValue(util::to_std(godot::String(
          cd->get_parent_class(godot::StringName(name.c_str())))));
      item["api_type"] = mcp::JsonValue(static_cast<int64_t>(
          cd->class_get_api_type(godot::StringName(name.c_str()))));
      results_arr.PushBack(std::move(item));
      count++;
    }
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(results_arr);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "documentation_search completed");
  return r;
}

mcp::JsonValue handle_get_method(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "documentation_get_method called");
  auto *cp = args.Find("class");
  auto *mp = args.Find("method");
  if (!cp || !cp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: class");
    return e;
  }
  if (!mp || !mp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: method");
    return e;
  }
  std::string class_name = cp->GetString();
  std::string method_name = mp->GetString();
  auto *cd = godot::ClassDBSingleton::get_singleton();
  if (!cd) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ClassDBSingleton not available");
    return e;
  }
  godot::StringName sn(class_name.c_str());
  if (!cd->class_exists(sn)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("class not found: " + class_name);
    return e;
  }
  godot::TypedArray<godot::Dictionary> methods =
      cd->class_get_method_list(sn, false);
  for (int i = 0; i < methods.size(); i++) {
    godot::Dictionary d = methods[i];
    if (d.has("name")) {
      godot::String mname = static_cast<godot::String>(d["name"]);
      if (util::to_std(mname) == method_name) {
        mcp::JsonValue result = variant_to_json(d);
        result["note"] = mcp::JsonValue("method signature without docstrings");
        mcp::JsonValue r(mcp::JsonValue::object_tag);
        r["result"] = std::move(result);
        LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                                  "documentation_get_method completed");
        return r;
      }
    }
  }
  mcp::JsonValue e(mcp::JsonValue::object_tag);
  e["error"] = mcp::JsonValue("method not found: " + method_name +
                              " in class " + class_name);
  return e;
}

mcp::JsonValue handle_get_property(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "documentation_get_property called");
  auto *cp = args.Find("class");
  auto *pp = args.Find("property");
  if (!cp || !cp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: class");
    return e;
  }
  if (!pp || !pp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: property");
    return e;
  }
  std::string class_name = cp->GetString();
  std::string prop_name = pp->GetString();
  auto *cd = godot::ClassDBSingleton::get_singleton();
  if (!cd) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ClassDBSingleton not available");
    return e;
  }
  godot::StringName sn(class_name.c_str());
  if (!cd->class_exists(sn)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("class not found: " + class_name);
    return e;
  }
  godot::TypedArray<godot::Dictionary> props =
      cd->class_get_property_list(sn, false);
  for (int i = 0; i < props.size(); i++) {
    godot::Dictionary d = props[i];
    if (d.has("name")) {
      godot::String pname = static_cast<godot::String>(d["name"]);
      if (util::to_std(pname) == prop_name) {
        mcp::JsonValue result = variant_to_json(d);
        mcp::JsonValue r(mcp::JsonValue::object_tag);
        r["result"] = std::move(result);
        LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                                  "documentation_get_property completed");
        return r;
      }
    }
  }
  mcp::JsonValue e(mcp::JsonValue::object_tag);
  e["error"] = mcp::JsonValue("property not found: " + prop_name +
                              " in class " + class_name);
  return e;
}

} // namespace doc_ops
} // namespace godot_self_driving
