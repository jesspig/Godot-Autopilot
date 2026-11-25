#ifndef GODOT_AUTOPILOT_JSON_VALUE_HPP
#define GODOT_AUTOPILOT_JSON_VALUE_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "util/owned_exception.hpp"

namespace godot_autopilot {
namespace util {

enum class JsonErrorCode { parse_error, deserialization_failed };

class JsonError : public RuntimeError {
 public:
  JsonError(JsonErrorCode code, std::string message);
  [[nodiscard]] JsonErrorCode Code() const noexcept;

 private:
  JsonErrorCode code_;
};

enum class DuplicateKeyPolicy { compat, strict };

class JsonValue {
 public:
  using Object = std::map<std::string, JsonValue, std::less<>>;
  using Array = std::vector<JsonValue>;

 private:
  std::variant<std::nullptr_t, bool, int64_t, double, std::string, Array, Object> data_;

 public:
  JsonValue();
  JsonValue(std::nullptr_t);
  JsonValue(bool);
  JsonValue(int);
  JsonValue(int64_t);
  JsonValue(double);
  JsonValue(const std::string&);
  JsonValue(std::string&&);
  JsonValue(const char*);
  JsonValue(const char*, size_t);
  JsonValue(Array);
  JsonValue(Object);

  enum ObjectTag { object_tag };
  enum ArrayTag { array_tag };
  JsonValue(ObjectTag);
  JsonValue(ArrayTag);

  ~JsonValue();
  JsonValue(const JsonValue& other);
  JsonValue& operator=(const JsonValue& other);
  JsonValue(JsonValue&& other) noexcept = default;
  JsonValue& operator=(JsonValue&& other) noexcept = default;

  static JsonValue Parse(std::string_view json,
                         DuplicateKeyPolicy policy = DuplicateKeyPolicy::compat);
  static std::string SanitizeUtf8(std::string_view text);

  std::string Dump(int indent = -1) const;

  [[nodiscard]] bool IsNull() const;
  [[nodiscard]] bool IsBool() const;
  [[nodiscard]] bool IsInt() const;
  [[nodiscard]] bool IsDouble() const;
  [[nodiscard]] bool IsNumber() const;
  [[nodiscard]] bool IsString() const;
  [[nodiscard]] bool IsArray() const;
  [[nodiscard]] bool IsObject() const;

  bool GetBool() const;
  int64_t GetInt() const;
  double GetDouble() const;
  const std::string& GetString() const;
  const Array& GetArray() const;
  Array& GetArray();
  const Object& GetObject() const;
  Object& GetObject();

  size_t Size() const;
  [[nodiscard]] bool Empty() const;
  [[nodiscard]] bool Contains(std::string_view key) const;

  JsonValue& operator[](size_t i);
  const JsonValue& operator[](size_t i) const;
  JsonValue& operator[](std::string_view key);
  const JsonValue& operator[](std::string_view key) const;

  const JsonValue* Find(std::string_view key) const;
  JsonValue* Find(std::string_view key);
  const JsonValue& At(std::string_view key) const;
  JsonValue& At(std::string_view key);

  void PushBack(JsonValue val);

  bool operator==(const JsonValue& other) const;
  bool operator!=(const JsonValue& other) const;

  Object::const_iterator begin() const;
  Object::const_iterator end() const;
  Object::iterator begin();
  Object::iterator end();
};

} // namespace util
} // namespace godot_autopilot

#endif
