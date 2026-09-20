#include <tools/tool_args.hpp>

#include <cmath>

namespace godot_autopilot {
namespace {

constexpr double kInt64LowerBound = -9223372036854775808.0;
constexpr double kInt64UpperBound = 9223372036854775808.0;

const mcp::JsonValue *find_present(const mcp::JsonValue &raw,
                                   const std::string &name) {
  const mcp::JsonValue *value = raw.Find(name);
  if (!value || value->IsNull())
    return nullptr;
  return value;
}

[[noreturn]] void throw_missing(const std::string &name) {
  throw ToolArgError("missing required parameter: " + name);
}

[[noreturn]] void throw_invalid(const std::string &name, const char *type_word) {
  throw ToolArgError("invalid parameter: " + name + " must be a " + type_word);
}

bool is_integral(double value) { return value == std::floor(value); }

bool fits_int64_range(double value) {
  return value >= kInt64LowerBound && value < kInt64UpperBound;
}

} // namespace

Args::Args(const mcp::JsonValue &raw,
           const std::vector<schema::ParamDef> &params)
    : raw_(raw), params_(params) {}

const mcp::JsonValue &Args::raw() const { return raw_; }

bool Args::has(const std::string &name) const {
  return find_present(raw_, name) != nullptr;
}

bool Args::is_null(const std::string &name) const {
  const mcp::JsonValue *value = raw_.Find(name);
  return value != nullptr && value->IsNull();
}

std::optional<std::string> Args::opt_string(const std::string &name) const {
  const mcp::JsonValue *value = find_present(raw_, name);
  if (!value)
    return std::nullopt;
  if (!value->IsString())
    throw_invalid(name, "string");
  return value->GetString();
}

std::optional<int64_t> Args::opt_int(const std::string &name) const {
  const mcp::JsonValue *value = find_present(raw_, name);
  if (!value)
    return std::nullopt;
  if (value->IsInt())
    return value->GetInt();
  if (value->IsDouble()) {
    const double number = value->GetDouble();
    if (is_integral(number) && fits_int64_range(number))
      return static_cast<int64_t>(number);
  }
  throw_invalid(name, "integer");
}

std::optional<double> Args::opt_number(const std::string &name) const {
  const mcp::JsonValue *value = find_present(raw_, name);
  if (!value)
    return std::nullopt;
  if (value->IsInt())
    return static_cast<double>(value->GetInt());
  if (value->IsDouble())
    return value->GetDouble();
  throw_invalid(name, "number");
}

std::optional<bool> Args::opt_bool(const std::string &name) const {
  const mcp::JsonValue *value = find_present(raw_, name);
  if (!value)
    return std::nullopt;
  if (!value->IsBool())
    throw_invalid(name, "boolean");
  return value->GetBool();
}

const mcp::JsonValue *Args::opt_object(const std::string &name) const {
  const mcp::JsonValue *value = find_present(raw_, name);
  if (!value)
    return nullptr;
  if (!value->IsObject())
    throw_invalid(name, "object");
  return value;
}

const mcp::JsonValue *Args::opt_array(const std::string &name) const {
  const mcp::JsonValue *value = find_present(raw_, name);
  if (!value)
    return nullptr;
  if (!value->IsArray())
    throw_invalid(name, "array");
  return value;
}

std::string Args::require_string(const std::string &name) const {
  const std::optional<std::string> value = opt_string(name);
  if (!value)
    throw_missing(name);
  return *value;
}

int64_t Args::require_int(const std::string &name) const {
  const std::optional<int64_t> value = opt_int(name);
  if (!value)
    throw_missing(name);
  return *value;
}

double Args::require_number(const std::string &name) const {
  const std::optional<double> value = opt_number(name);
  if (!value)
    throw_missing(name);
  return *value;
}

bool Args::require_bool(const std::string &name) const {
  const std::optional<bool> value = opt_bool(name);
  if (!value)
    throw_missing(name);
  return *value;
}

const mcp::JsonValue &Args::require_object(const std::string &name) const {
  const mcp::JsonValue *value = opt_object(name);
  if (!value)
    throw_missing(name);
  return *value;
}

const mcp::JsonValue &Args::require_array(const std::string &name) const {
  const mcp::JsonValue *value = opt_array(name);
  if (!value)
    throw_missing(name);
  return *value;
}

std::string Args::get_string(const std::string &name,
                             const std::string &fallback) const {
  const std::optional<std::string> value = opt_string(name);
  return value ? *value : fallback;
}

int64_t Args::get_int(const std::string &name, int64_t fallback) const {
  const std::optional<int64_t> value = opt_int(name);
  return value ? *value : fallback;
}

double Args::get_number(const std::string &name, double fallback) const {
  const std::optional<double> value = opt_number(name);
  return value ? *value : fallback;
}

bool Args::get_bool(const std::string &name, bool fallback) const {
  const std::optional<bool> value = opt_bool(name);
  return value ? *value : fallback;
}

void Args::reject_unknown() const {
  if (!raw_.IsObject())
    return;
  for (const auto &entry : raw_.GetObject()) {
    bool known = false;
    for (const auto &param : params_) {
      if (param.name == entry.first) {
        known = true;
        break;
      }
    }
    if (!known)
      throw ToolArgError("unknown parameter: " + entry.first);
  }
}

} // namespace godot_autopilot
