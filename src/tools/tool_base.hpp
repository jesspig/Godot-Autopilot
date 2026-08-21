#ifndef GODOT_AUTOPILOT_TOOL_BASE_HPP
#define GODOT_AUTOPILOT_TOOL_BASE_HPP

#include <initializer_list>
#include <mcp/JsonValue.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <tools/schema_builder.hpp>
#include <util/error_util.hpp>

namespace godot_autopilot {

enum class SideEffect { None, WritesFile, WritesConfig, ShowsAlert, ModifiesWindow, Process };

inline const char* side_effect_name(SideEffect e) {
  switch (e) {
    case SideEffect::None: return "";
    case SideEffect::WritesFile: return "writes_file";
    case SideEffect::WritesConfig: return "writes_config";
    case SideEffect::ShowsAlert: return "shows_alert";
    case SideEffect::ModifiesWindow: return "modifies_window";
    case SideEffect::Process: return "process";
  }
  return "";
}

struct ToolMeta {
  std::string name;
  std::string description;
  std::string category;
  std::vector<std::string> tags;
  bool basic_schema;
};

class IExportGuard {
public:
  virtual ~IExportGuard() = default;
  virtual bool blocked_during_export() const = 0;
};

class IAsync {
public:
  virtual ~IAsync() = default;
  virtual bool is_async() const = 0;
};

class ISideEffect {
public:
  virtual ~ISideEffect() = default;
  virtual SideEffect side_effects() const = 0;
};

class IMetaTool {
public:
  virtual ~IMetaTool() = default;
};

class ToolBase {
public:
  virtual ~ToolBase() = default;
  virtual const ToolMeta &meta() const = 0;
  virtual mcp::JsonValue execute(const mcp::JsonValue &args) = 0;
  virtual mcp::JsonValue input_schema() const = 0;
};

inline mcp::JsonValue make_ok() { return mcp::JsonValue(mcp::JsonValue::object_tag); }

inline mcp::JsonValue make_error(const std::string &msg) { return util::error_json(msg); }

inline bool blocks_export(const ToolBase &t) {
  const auto *guard = dynamic_cast<const IExportGuard *>(&t);
  return guard ? guard->blocked_during_export() : true;
}

inline bool tool_is_async(const ToolBase &t) {
  const auto *async = dynamic_cast<const IAsync *>(&t);
  return async ? async->is_async() : false;
}

inline SideEffect side_effect_of(const ToolBase &t) {
  const auto *side = dynamic_cast<const ISideEffect *>(&t);
  return side ? side->side_effects() : SideEffect::None;
}

class ArgReader {
public:
  explicit ArgReader(const mcp::JsonValue &args) : args_(args) {}

  bool has(const std::string &name) const { return args_.Find(name) != nullptr; }

  const mcp::JsonValue *find(const std::string &name) const { return args_.Find(name); }

  std::optional<std::string> str(const std::string &name) const {
    const auto *v = find(name);
    if (v != nullptr && v->IsString()) return v->GetString();
    return std::nullopt;
  }

  std::string str(const std::string &name, const std::string &def) const {
    auto value = str(name);
    return value ? std::move(*value) : def;
  }

  std::optional<std::string> require_str(const std::string &name) const {
    if (has(name)) return str(name);
    return std::nullopt;
  }

  std::optional<int64_t> integer(const std::string &name) const {
    const auto *v = find(name);
    if (v != nullptr && v->IsInt()) return v->GetInt();
    return std::nullopt;
  }

  int64_t integer(const std::string &name, int64_t def) const {
    auto value = integer(name);
    return value ? *value : def;
  }

  std::optional<bool> boolean(const std::string &name) const {
    const auto *v = find(name);
    if (v != nullptr && v->IsBool()) return v->GetBool();
    return std::nullopt;
  }

  bool boolean(const std::string &name, bool def) const {
    auto value = boolean(name);
    return value ? *value : def;
  }

  std::optional<double> number(const std::string &name) const {
    const auto *v = find(name);
    if (v != nullptr && v->IsNumber()) return v->GetDouble();
    return std::nullopt;
  }

  double number(const std::string &name, double def) const {
    auto value = number(name);
    return value ? *value : def;
  }

private:
  const mcp::JsonValue &args_;
};

} // namespace godot_autopilot

#endif