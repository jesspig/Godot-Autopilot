#include "util/json_value.hpp"

#include <rapidjson/error/en.h>
#include <rapidjson/reader.h>
#include <rapidjson/stream.h>

#include <array>
#include <cerrno>
#include <charconv>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace util {

namespace {

constexpr int kMaxDepth = 1000;

const char* JsonTypeName(const JsonValue& value) noexcept {
  if (value.IsNull()) return "null";
  if (value.IsBool()) return "bool";
  if (value.IsInt()) return "int";
  if (value.IsDouble()) return "double";
  if (value.IsString()) return "string";
  if (value.IsArray()) return "array";
  return "object";
}

[[noreturn]] void ThrowParse(const std::string& reason) {
  throw JsonError(JsonErrorCode::parse_error, "JSON parse error: " + reason);
}

std::string FormatDouble(double d) {
#if defined(_MSC_VER) || (defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE >= 11) || defined(__cpp_lib_to_chars)
  std::array<char, 64> buf;
  auto res = std::to_chars(buf.data(), buf.data() + buf.size(), d, std::chars_format::general);
  if (res.ec == std::errc()) return std::string(buf.data(), res.ptr);
#endif
  std::array<char, 64> fallback_buf;
  int len = std::snprintf(fallback_buf.data(), fallback_buf.size(), "%.*g",
                          std::numeric_limits<double>::max_digits10, d);
  return std::string(fallback_buf.data(), static_cast<size_t>(len));
}

void AppendEscapedString(std::string& out, const std::string& s) {
  out.push_back('"');
  static constexpr char kHexDigits[] = "0123456789abcdef";
  size_t run_start = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    const char* esc = nullptr;
    switch (c) {
      case '"': esc = "\\\""; break;
      case '\\': esc = "\\\\"; break;
      case '\b': esc = "\\b"; break;
      case '\f': esc = "\\f"; break;
      case '\n': esc = "\\n"; break;
      case '\r': esc = "\\r"; break;
      case '\t': esc = "\\t"; break;
      default:
        if (c < 0x20) {
          out.append(s, run_start, i - run_start);
          out += "\\u00";
          out.push_back(static_cast<char>('0' + (c >> 4)));
          out.push_back(kHexDigits[c & 0xF]);
          run_start = i + 1;
        }
        continue;
    }
    out.append(s, run_start, i - run_start);
    out += esc;
    run_start = i + 1;
  }
  out.append(s, run_start, s.size() - run_start);
  out.push_back('"');
}

size_t EstimateSize(const JsonValue& value, int indent) {
  if (value.IsString()) return value.GetString().size() + 8;
  if (value.IsArray()) {
    const auto& arr = value.GetArray();
    if (arr.empty()) return 2;
    size_t n = arr.size() * 4 + 2;
    if (indent >= 0) n += arr.size() * static_cast<size_t>(indent);
    for (const auto& el : arr) n += EstimateSize(el, indent);
    return n;
  }
  if (value.IsObject()) {
    const auto& obj = value.GetObject();
    if (obj.empty()) return 2;
    size_t n = obj.size() * 8 + 2;
    if (indent >= 0) n += obj.size() * static_cast<size_t>(indent);
    for (const auto& entry : obj) n += entry.first.size() + 3 + EstimateSize(entry.second, indent);
    return n;
  }
  if (value.IsInt()) return 24;
  if (value.IsDouble()) return 32;
  return 8;
}

void DumpValue(std::string& out, const JsonValue& value, int indent, int depth) {
  auto indent_at = [&](int level) {
    int n = level * indent;
    if (n > 0) out.append(static_cast<size_t>(n), ' ');
  };

  if (value.IsNull()) {
    out += "null";
  } else if (value.IsBool()) {
    out += value.GetBool() ? "true" : "false";
  } else if (value.IsInt()) {
    out += std::to_string(value.GetInt());
  } else if (value.IsDouble()) {
    double d = value.GetDouble();
    if (std::isfinite(d)) {
      std::string repr = FormatDouble(d);
      out += repr;
      if (repr.find_first_of(".eE") == std::string_view::npos) out += ".0";
    } else {
      out += "null";
    }
  } else if (value.IsString()) {
    AppendEscapedString(out, value.GetString());
  } else if (value.IsArray()) {
    const auto& arr = value.GetArray();
    if (arr.empty()) {
      out += "[]";
    } else if (indent < 0) {
      out.push_back('[');
      for (size_t i = 0; i < arr.size(); ++i) {
        if (i > 0) out.push_back(',');
        DumpValue(out, arr[i], indent, depth);
      }
      out.push_back(']');
    } else {
      out += "[\n";
      for (size_t i = 0; i < arr.size(); ++i) {
        indent_at(depth + 1);
        DumpValue(out, arr[i], indent, depth + 1);
        if (i + 1 < arr.size()) out.push_back(',');
        out.push_back('\n');
      }
      indent_at(depth);
      out.push_back(']');
    }
  } else {
    const auto& obj = value.GetObject();
    if (obj.empty()) {
      out += "{}";
    } else if (indent < 0) {
      out.push_back('{');
      bool first = true;
      for (const auto& entry : obj) {
        if (!first) out.push_back(',');
        first = false;
        AppendEscapedString(out, entry.first);
        out.push_back(':');
        DumpValue(out, entry.second, indent, depth);
      }
      out.push_back('}');
    } else {
      out += "{\n";
      bool first = true;
      for (const auto& entry : obj) {
        if (!first) out += ",\n";
        first = false;
        indent_at(depth + 1);
        AppendEscapedString(out, entry.first);
        out.push_back(':');
        out.push_back(' ');
        DumpValue(out, entry.second, indent, depth + 1);
      }
      out.push_back('\n');
      indent_at(depth);
      out.push_back('}');
    }
  }
}

double ParseFloatFallback(std::string_view token) {
  std::string adjusted(token);
  struct lconv* lc = std::localeconv();
  const char* dp = lc == nullptr ? "." : lc->decimal_point;
  if (dp == nullptr || dp[0] == '\0') dp = ".";
  size_t dp_len = std::strlen(dp);
  if (dp_len != 1 || dp[0] != '.') {
    std::string converted;
    converted.reserve(adjusted.size());
    for (char c : adjusted) {
      if (c == '.') {
        converted.append(dp, dp_len);
      } else {
        converted.push_back(c);
      }
    }
    adjusted = std::move(converted);
  }
  char* end = nullptr;
  errno = 0;
  double value = std::strtod(adjusted.c_str(), &end);
  if (end != adjusted.c_str() + adjusted.size()) ThrowParse("invalid number");
  if (errno == ERANGE) {
    if (value == 0.0 || std::abs(value) < std::numeric_limits<double>::min()) return value;
    ThrowParse("number out of range");
  }
  return value;
}

JsonValue ParseNumberToken(std::string_view token) {
  const char* first = token.data();
  const char* last = first + token.size();
  bool is_float = token.find_first_of(".eE") != std::string_view::npos;
  if (!is_float) {
    int64_t int_val = 0;
    auto res = std::from_chars(first, last, int_val);
    if (res.ec == std::errc()) return JsonValue(int_val);
    if (res.ec == std::errc::result_out_of_range) {
      if (!token.empty() && token[0] == '-') ThrowParse("integer out of range");
      uint64_t uint_val = 0;
      auto res2 = std::from_chars(first, last, uint_val);
      if (res2.ec == std::errc()) {
        if (uint_val > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
          throw JsonError(JsonErrorCode::deserialization_failed,
                          "JSON parse error: uint64 value out of int64 range: " +
                              std::to_string(uint_val));
        }
        return JsonValue(static_cast<int64_t>(uint_val));
      }
      ThrowParse("integer out of range");
    }
    ThrowParse("invalid number");
  }
  double double_val = 0;
  auto res = std::from_chars(first, last, double_val);
  if (res.ec == std::errc()) {
    if (!std::isfinite(double_val)) ThrowParse("number out of range");
    return JsonValue(double_val);
  }
  if (res.ec == std::errc::result_out_of_range) return JsonValue(ParseFloatFallback(token));
  ThrowParse("invalid number");
}

class BuildHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, BuildHandler> {
 public:
  explicit BuildHandler(DuplicateKeyPolicy policy) : policy_(policy) {}

  bool Null() {
    Emit(JsonValue(nullptr));
    return true;
  }
  bool Bool(bool b) {
    Emit(JsonValue(b));
    return true;
  }
  bool Int(int i) {
    Emit(JsonValue(static_cast<int64_t>(i)));
    return true;
  }
  bool Uint(unsigned i) {
    Emit(JsonValue(static_cast<int64_t>(i)));
    return true;
  }
  bool Int64(int64_t i) {
    Emit(JsonValue(i));
    return true;
  }
  bool Uint64(uint64_t i) {
    if (i > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
      throw JsonError(JsonErrorCode::deserialization_failed,
                      "JSON parse error: uint64 value out of int64 range: " + std::to_string(i));
    }
    Emit(JsonValue(static_cast<int64_t>(i)));
    return true;
  }
  bool Double(double d) {
    if (!std::isfinite(d)) ThrowParse("number out of range");
    Emit(JsonValue(d));
    return true;
  }
  bool RawNumber(const char* str, rapidjson::SizeType length, bool) {
    Emit(ParseNumberToken(std::string_view(str, length)));
    return true;
  }
  bool String(const char* str, rapidjson::SizeType length, bool) {
    Emit(JsonValue(std::string(str, length)));
    return true;
  }
  bool StartObject() {
    Push(true);
    return true;
  }
  bool Key(const char* str, rapidjson::SizeType length, bool) {
    Frame& frame = stack_.back();
    frame.segment.assign(str, length);
    if (policy_ == DuplicateKeyPolicy::strict &&
        frame.object.find(frame.segment) != frame.object.end()) {
      throw JsonError(JsonErrorCode::parse_error,
                      "JSON parse error: duplicate key at path " + Path());
    }
    return true;
  }
  bool EndObject(rapidjson::SizeType) {
    Frame frame = std::move(stack_.back());
    stack_.pop_back();
    Emit(JsonValue(std::move(frame.object)));
    return true;
  }
  bool StartArray() {
    Push(false);
    return true;
  }
  bool EndArray(rapidjson::SizeType) {
    Frame frame = std::move(stack_.back());
    stack_.pop_back();
    Emit(JsonValue(std::move(frame.array)));
    return true;
  }

  JsonValue TakeResult() { return std::move(root_); }

 private:
  struct Frame {
    bool is_object = false;
    JsonValue::Array array;
    JsonValue::Object object;
    std::string segment;
  };

  void Push(bool is_object) {
    if (stack_.size() >= static_cast<size_t>(kMaxDepth)) {
      ThrowParse("nesting depth exceeds " + std::to_string(kMaxDepth));
    }
    Frame frame;
    frame.is_object = is_object;
    if (!stack_.empty() && !stack_.back().is_object) {
      stack_.back().segment = "[" + std::to_string(stack_.back().array.size()) + "]";
    }
    stack_.push_back(std::move(frame));
  }

  void Emit(JsonValue child) {
    if (stack_.empty()) {
      root_ = std::move(child);
      return;
    }
    Frame& frame = stack_.back();
    if (frame.is_object) {
      frame.object.emplace(frame.segment, std::move(child));
    } else {
      frame.array.push_back(std::move(child));
    }
  }

  std::string Path() const {
    std::string path = "$";
    for (const Frame& frame : stack_) {
      if (frame.segment.empty()) continue;
      if (frame.segment.front() == '[') {
        path += frame.segment;
      } else {
        path += '.';
        path += frame.segment;
      }
    }
    return path;
  }

  DuplicateKeyPolicy policy_;
  std::vector<Frame> stack_;
  JsonValue root_;
};

} // namespace

JsonError::JsonError(JsonErrorCode code, std::string message)
    : RuntimeError(std::move(message)), code_(code) {}

JsonErrorCode JsonError::Code() const noexcept { return code_; }

JsonValue::JsonValue() : data_(nullptr) {}
JsonValue::JsonValue(std::nullptr_t) : data_(nullptr) {}
JsonValue::JsonValue(bool v) : data_(v) {}
JsonValue::JsonValue(int v) : data_(static_cast<int64_t>(v)) {}
JsonValue::JsonValue(int64_t v) : data_(v) {}
JsonValue::JsonValue(double v) : data_(v) {}
JsonValue::JsonValue(const std::string& v) : data_(v) {}
JsonValue::JsonValue(std::string&& v) : data_(std::move(v)) {}
JsonValue::JsonValue(const char* v) : data_(std::string(v)) {}
JsonValue::JsonValue(const char* v, size_t len) : data_(std::string(v, len)) {}
JsonValue::JsonValue(Array v) : data_(std::move(v)) {}
JsonValue::JsonValue(Object v) : data_(std::move(v)) {}
JsonValue::JsonValue(ObjectTag) : data_(Object{}) {}
JsonValue::JsonValue(ArrayTag) : data_(Array{}) {}

JsonValue::~JsonValue() {
  if (IsArray()) {
    if (std::get<Array>(data_).empty()) {
      return;
    }
  } else if (IsObject()) {
    if (std::get<Object>(data_).empty()) {
      return;
    }
  } else {
    return;
  }
  std::vector<JsonValue> stack;
  if (IsArray()) {
    Array moved = std::move(std::get<Array>(data_));
    data_.emplace<std::nullptr_t>(nullptr);
    stack.reserve(moved.size() + 1);
    for (auto& child : moved) {
      stack.push_back(std::move(child));
    }
  } else {
    Object moved = std::move(std::get<Object>(data_));
    data_.emplace<std::nullptr_t>(nullptr);
    stack.reserve(moved.size() + 1);
    for (auto& entry : moved) {
      stack.push_back(std::move(entry.second));
    }
  }
  while (!stack.empty()) {
    JsonValue node = std::move(stack.back());
    stack.pop_back();
    if (node.IsArray()) {
      Array& arr = std::get<Array>(node.data_);
      for (auto& child : arr) {
        stack.push_back(std::move(child));
      }
    } else if (node.IsObject()) {
      Object& obj = std::get<Object>(node.data_);
      for (auto& entry : obj) {
        stack.push_back(std::move(entry.second));
      }
    }
  }
}

JsonValue::JsonValue(const JsonValue& other) : data_(nullptr) {
  if (!other.IsArray() && !other.IsObject()) {
    data_ = other.data_;
    return;
  }
  std::vector<std::pair<const JsonValue*, JsonValue*>> stack;
  if (other.IsArray()) {
    const Array& src = std::get<Array>(other.data_);
    data_.emplace<Array>();
    Array& dst = std::get<Array>(data_);
    dst.reserve(src.size());
    for (size_t i = 0; i < src.size(); ++i) {
      dst.emplace_back(nullptr);
    }
    stack.reserve(src.size());
    for (size_t i = 0; i < src.size(); ++i) {
      stack.emplace_back(&src[i], &dst[i]);
    }
  } else {
    const Object& src = std::get<Object>(other.data_);
    data_.emplace<Object>();
    Object& dst = std::get<Object>(data_);
    stack.reserve(src.size());
    for (const auto& entry : src) {
      auto pos = dst.emplace(entry.first, nullptr).first;
      stack.emplace_back(&entry.second, &pos->second);
    }
  }
  while (!stack.empty()) {
    auto work = stack.back();
    stack.pop_back();
    const JsonValue* src = work.first;
    JsonValue* dst = work.second;
    if (!src->IsArray() && !src->IsObject()) {
      dst->data_ = src->data_;
    } else if (src->IsArray()) {
      const Array& sarr = std::get<Array>(src->data_);
      dst->data_.emplace<Array>();
      Array& darr = std::get<Array>(dst->data_);
      darr.reserve(sarr.size());
      for (size_t i = 0; i < sarr.size(); ++i) {
        darr.emplace_back(nullptr);
      }
      for (size_t i = 0; i < sarr.size(); ++i) {
        stack.emplace_back(&sarr[i], &darr[i]);
      }
    } else {
      const Object& sobj = std::get<Object>(src->data_);
      dst->data_.emplace<Object>();
      Object& dobj = std::get<Object>(dst->data_);
      for (const auto& entry : sobj) {
        auto pos = dobj.emplace(entry.first, nullptr).first;
        stack.emplace_back(&entry.second, &pos->second);
      }
    }
  }
}

JsonValue& JsonValue::operator=(const JsonValue& other) {
  if (this == &other) {
    return *this;
  }
  JsonValue tmp(other);
  data_.swap(tmp.data_);
  return *this;
}

JsonValue JsonValue::Parse(std::string_view json, DuplicateKeyPolicy policy) {
  if (json.empty()) ThrowParse("unexpected end of input");
  if (json.find('\0') != std::string_view::npos) ThrowParse("unexpected character");
  rapidjson::StringStream stream(json.data());
  BuildHandler handler(policy);
  rapidjson::Reader reader;
  constexpr unsigned flags = rapidjson::kParseValidateEncodingFlag |
                             rapidjson::kParseNumbersAsStringsFlag;
  if (!reader.Parse<flags>(stream, handler)) {
    throw JsonError(JsonErrorCode::parse_error,
                    "JSON parse error: " +
                        std::string(rapidjson::GetParseError_En(reader.GetParseErrorCode())) +
                        " at offset " + std::to_string(reader.GetErrorOffset()));
  }
  return handler.TakeResult();
}

std::string JsonValue::SanitizeUtf8(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  size_t i = 0;
  while (i < text.size()) {
    unsigned char b0 = static_cast<unsigned char>(text[i]);
    if (b0 < 0x80) {
      out.push_back(static_cast<char>(b0));
      ++i;
      continue;
    }
    size_t cont = 0;
    unsigned char lo = 0x80;
    unsigned char hi = 0xBF;
    bool lead_ok = true;
    if (b0 >= 0xC2 && b0 <= 0xDF) {
      cont = 1;
    } else if (b0 >= 0xE0 && b0 <= 0xEF) {
      cont = 2;
      if (b0 == 0xE0) lo = 0xA0;
      else if (b0 == 0xED) hi = 0x9F;
    } else if (b0 >= 0xF0 && b0 <= 0xF4) {
      cont = 3;
      if (b0 == 0xF0) lo = 0x90;
      else if (b0 == 0xF4) hi = 0x8F;
    } else {
      lead_ok = false;
    }
    bool seq_ok = lead_ok && i + cont < text.size();
    if (seq_ok) {
      unsigned char b1 = static_cast<unsigned char>(text[i + 1]);
      seq_ok = b1 >= lo && b1 <= hi;
      for (size_t k = 2; seq_ok && k <= cont; ++k) {
        unsigned char c = static_cast<unsigned char>(text[i + k]);
        seq_ok = (c & 0xC0) == 0x80;
      }
    }
    if (seq_ok) {
      out.append(text.data() + i, cont + 1);
      i += cont + 1;
    } else {
      out += "\xEF\xBF\xBD";
      ++i;
    }
  }
  return out;
}

std::string JsonValue::Dump(int indent) const {
  std::string out;
  out.reserve(EstimateSize(*this, indent));
  DumpValue(out, *this, indent, 0);
  return out;
}

bool JsonValue::IsNull() const { return std::holds_alternative<std::nullptr_t>(data_); }
bool JsonValue::IsBool() const { return std::holds_alternative<bool>(data_); }
bool JsonValue::IsInt() const { return std::holds_alternative<int64_t>(data_); }
bool JsonValue::IsDouble() const { return std::holds_alternative<double>(data_); }
bool JsonValue::IsNumber() const { return IsInt() || IsDouble(); }
bool JsonValue::IsString() const { return std::holds_alternative<std::string>(data_); }
bool JsonValue::IsArray() const { return std::holds_alternative<Array>(data_); }
bool JsonValue::IsObject() const { return std::holds_alternative<Object>(data_); }

bool JsonValue::GetBool() const {
  if (!IsBool()) {
    throw JsonError(JsonErrorCode::deserialization_failed,
                    std::string("JsonValue::GetBool: expected bool, got ") + JsonTypeName(*this));
  }
  return std::get<bool>(data_);
}

int64_t JsonValue::GetInt() const {
  if (!IsInt()) {
    throw JsonError(JsonErrorCode::deserialization_failed,
                    std::string("JsonValue::GetInt: expected int, got ") + JsonTypeName(*this));
  }
  return std::get<int64_t>(data_);
}

double JsonValue::GetDouble() const {
  if (!IsDouble()) {
    throw JsonError(JsonErrorCode::deserialization_failed,
                    std::string("JsonValue::GetDouble: expected double, got ") + JsonTypeName(*this));
  }
  return std::get<double>(data_);
}

const std::string& JsonValue::GetString() const {
  if (!IsString()) {
    throw JsonError(JsonErrorCode::deserialization_failed,
                    std::string("JsonValue::GetString: expected string, got ") + JsonTypeName(*this));
  }
  return std::get<std::string>(data_);
}

const JsonValue::Array& JsonValue::GetArray() const {
  if (!IsArray()) {
    throw JsonError(JsonErrorCode::deserialization_failed,
                    std::string("JsonValue::GetArray: expected array, got ") + JsonTypeName(*this));
  }
  return std::get<Array>(data_);
}

JsonValue::Array& JsonValue::GetArray() {
  if (!IsArray()) {
    throw JsonError(JsonErrorCode::deserialization_failed,
                    std::string("JsonValue::GetArray: expected array, got ") + JsonTypeName(*this));
  }
  return std::get<Array>(data_);
}

const JsonValue::Object& JsonValue::GetObject() const {
  if (!IsObject()) {
    throw JsonError(JsonErrorCode::deserialization_failed,
                    std::string("JsonValue::GetObject: expected object, got ") + JsonTypeName(*this));
  }
  return std::get<Object>(data_);
}

JsonValue::Object& JsonValue::GetObject() {
  if (!IsObject()) {
    throw JsonError(JsonErrorCode::deserialization_failed,
                    std::string("JsonValue::GetObject: expected object, got ") + JsonTypeName(*this));
  }
  return std::get<Object>(data_);
}

size_t JsonValue::Size() const {
  if (IsArray()) return GetArray().size();
  if (IsObject()) return GetObject().size();
  if (IsString()) return GetString().size();
  return 0;
}

bool JsonValue::Empty() const {
  if (IsArray()) return GetArray().empty();
  if (IsObject()) return GetObject().empty();
  return IsNull();
}

bool JsonValue::Contains(std::string_view key) const {
  return IsObject() && GetObject().find(key) != GetObject().end();
}

JsonValue& JsonValue::operator[](size_t i) { return GetArray()[i]; }
const JsonValue& JsonValue::operator[](size_t i) const { return GetArray()[i]; }

JsonValue& JsonValue::operator[](std::string_view key) {
  auto& obj = GetObject();
  auto pos = obj.emplace(std::string(key), nullptr).first;
  return pos->second;
}

const JsonValue& JsonValue::operator[](std::string_view key) const {
  auto it = GetObject().find(key);
  if (it == GetObject().end()) {
    throw JsonError(JsonErrorCode::deserialization_failed,
                    "JsonValue: key not found: '" + std::string(key) + "'");
  }
  return it->second;
}

const JsonValue* JsonValue::Find(std::string_view key) const {
  if (!IsObject()) return nullptr;
  auto it = GetObject().find(key);
  return it != GetObject().end() ? &it->second : nullptr;
}

JsonValue* JsonValue::Find(std::string_view key) {
  if (!IsObject()) return nullptr;
  auto it = GetObject().find(key);
  return it != GetObject().end() ? &it->second : nullptr;
}

const JsonValue& JsonValue::At(std::string_view key) const {
  auto it = GetObject().find(key);
  if (it == GetObject().end()) {
    throw JsonError(JsonErrorCode::deserialization_failed,
                    "JsonValue::At: key not found: '" + std::string(key) + "'");
  }
  return it->second;
}

JsonValue& JsonValue::At(std::string_view key) {
  auto it = GetObject().find(key);
  if (it == GetObject().end()) {
    throw JsonError(JsonErrorCode::deserialization_failed,
                    "JsonValue::At: key not found: '" + std::string(key) + "'");
  }
  return it->second;
}

void JsonValue::PushBack(JsonValue val) { GetArray().push_back(std::move(val)); }

bool JsonValue::operator==(const JsonValue& other) const { return data_ == other.data_; }
bool JsonValue::operator!=(const JsonValue& other) const { return !(*this == other); }

JsonValue::Object::const_iterator JsonValue::begin() const { return GetObject().begin(); }
JsonValue::Object::const_iterator JsonValue::end() const { return GetObject().end(); }
JsonValue::Object::iterator JsonValue::begin() { return GetObject().begin(); }
JsonValue::Object::iterator JsonValue::end() { return GetObject().end(); }

} // namespace util
} // namespace godot_autopilot
