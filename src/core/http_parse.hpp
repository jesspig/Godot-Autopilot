#ifndef GODOT_AUTOPILOT_HTTP_PARSE_HPP
#define GODOT_AUTOPILOT_HTTP_PARSE_HPP

#include <cstddef>
#include <string>
#include <unordered_map>

namespace godot_autopilot {
namespace http_parse {

constexpr size_t kMaxRequestLineBytes = 8 * 1024;
constexpr size_t kMaxHeaderBlockBytes = 16 * 1024;
constexpr size_t kMaxBodyBytes = 4 * 1024 * 1024;
constexpr const char *kQueryRawHeader = "x-gda-query";
constexpr const char *kQueryHeaderPrefix = "x-gda-query-";

enum class BodyDecision { kAccept, kTooLarge, kMalformed };

struct RequestLine {
  bool ok{false};
  std::string method;
  std::string target;
  std::string path;
  std::string query;
  std::unordered_map<std::string, std::string> query_params;
  std::string version;
  std::string error;
};

struct HeaderBlock {
  bool ok{false};
  std::unordered_map<std::string, std::string> headers;
  std::string error;
};

inline bool is_token_char(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
         c == '+' || c == '*';
}

inline std::string to_lower(const std::string &text) {
  std::string result = text;
  for (char &c : result) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c + ('a' - 'A'));
    }
  }
  return result;
}

inline std::string trim(const std::string &text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) {
    ++begin;
  }
  while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t')) {
    --end;
  }
  return text.substr(begin, end - begin);
}

inline int hex_digit_value(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

inline std::string percent_decode(const std::string &text) {
  std::string result;
  result.reserve(text.size());
  for (size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '%' && index + 2 < text.size()) {
      const int high = hex_digit_value(text[index + 1]);
      const int low = hex_digit_value(text[index + 2]);
      if (high >= 0 && low >= 0) {
        result.push_back(static_cast<char>((high << 4) | low));
        index += 2;
        continue;
      }
    }
    result.push_back(text[index]);
  }
  return result;
}

inline std::unordered_map<std::string, std::string>
parse_query(const std::string &query) {
  std::unordered_map<std::string, std::string> params;
  size_t cursor = 0;
  while (cursor <= query.size()) {
    const size_t separator = query.find('&', cursor);
    const size_t end = separator == std::string::npos ? query.size() : separator;
    if (end > cursor) {
      const std::string pair = query.substr(cursor, end - cursor);
      const size_t equals = pair.find('=');
      if (equals == std::string::npos) {
        params[percent_decode(pair)] = std::string();
      } else {
        params[percent_decode(pair.substr(0, equals))] =
            percent_decode(pair.substr(equals + 1));
      }
    }
    if (separator == std::string::npos) {
      break;
    }
    cursor = separator + 1;
  }
  return params;
}

inline bool parse_request_line(const std::string &line, RequestLine &out) {
  out = RequestLine();
  const std::string trimmed = trim(line);
  if (trimmed.empty()) {
    out.error = "empty request line";
    return false;
  }
  if (trimmed.size() > kMaxRequestLineBytes) {
    out.error = "request line exceeds the length limit";
    return false;
  }
  const size_t first_space = trimmed.find(' ');
  if (first_space == std::string::npos) {
    out.error = "request line has no target";
    return false;
  }
  const size_t second_space = trimmed.find(' ', first_space + 1);
  if (second_space == std::string::npos) {
    out.error = "request line has no version";
    return false;
  }
  if (trimmed.find(' ', second_space + 1) != std::string::npos) {
    out.error = "request line has extra fields";
    return false;
  }
  const std::string method = trimmed.substr(0, first_space);
  const std::string target =
      trimmed.substr(first_space + 1, second_space - first_space - 1);
  const std::string version = trimmed.substr(second_space + 1);
  if (method.empty()) {
    out.error = "request line has no method";
    return false;
  }
  for (char c : method) {
    if (!is_token_char(c)) {
      out.error = "request line method is not a token";
      return false;
    }
  }
  if (target.empty() || target[0] != '/') {
    out.error = "request target is not an absolute path";
    return false;
  }
  if (version.size() < 5 || version.compare(0, 5, "HTTP/") != 0) {
    out.error = "request line version is not http";
    return false;
  }
  out.method = method;
  out.target = target;
  out.version = version;
  const size_t question = target.find('?');
  if (question == std::string::npos) {
    out.path = target;
    out.query.clear();
  } else {
    out.path = target.substr(0, question);
    out.query = target.substr(question + 1);
  }
  out.query_params = parse_query(out.query);
  out.ok = true;
  return true;
}

inline bool parse_headers(const std::string &block, HeaderBlock &out) {
  out = HeaderBlock();
  if (block.size() > kMaxHeaderBlockBytes) {
    out.error = "header block exceeds the length limit";
    return false;
  }
  size_t cursor = 0;
  while (cursor < block.size()) {
    size_t end = block.find('\n', cursor);
    if (end == std::string::npos) {
      end = block.size();
    }
    std::string line = block.substr(cursor, end - cursor);
    cursor = end + 1;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }
    const size_t colon = line.find(':');
    if (colon == std::string::npos) {
      out.error = "header line has no colon";
      return false;
    }
    const std::string name = trim(line.substr(0, colon));
    if (name.empty()) {
      out.error = "header name is empty";
      return false;
    }
    for (char c : name) {
      if (!is_token_char(c)) {
        out.error = "header name is not a token";
        return false;
      }
    }
    out.headers[to_lower(name)] = trim(line.substr(colon + 1));
  }
  out.ok = true;
  return true;
}

inline bool parse_content_length(const HeaderBlock &headers, size_t &out) {
  const auto found = headers.headers.find("content-length");
  if (found == headers.headers.end()) {
    out = 0;
    return true;
  }
  const std::string &raw = found->second;
  if (raw.empty()) {
    return false;
  }
  size_t value = 0;
  for (char c : raw) {
    if (c < '0' || c > '9') {
      return false;
    }
    const size_t digit = static_cast<size_t>(c - '0');
    if (value > (static_cast<size_t>(-1) - digit) / 10) {
      return false;
    }
    value = value * 10 + digit;
  }
  out = value;
  return true;
}

inline BodyDecision decide_body(size_t content_length) {
  if (content_length > kMaxBodyBytes) {
    return BodyDecision::kTooLarge;
  }
  return BodyDecision::kAccept;
}

inline bool request_keeps_alive(const std::string &version,
                                const HeaderBlock &headers) {
  const auto found = headers.headers.find("connection");
  if (found != headers.headers.end()) {
    const std::string value = to_lower(found->second);
    if (value.find("close") != std::string::npos) {
      return false;
    }
    if (value.find("keep-alive") != std::string::npos) {
      return true;
    }
  }
  return version != "HTTP/1.0";
}

} // namespace http_parse
} // namespace godot_autopilot

#endif
