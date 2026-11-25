#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace control {

struct Artifact {
  std::string id;
  std::string payload;
  std::string digest;
  std::size_t bytes = 0;
};

inline std::string fnv1a_hex(const std::string &text) {
  std::uint64_t hash = 14695981039346656037ULL;
  const std::uint64_t prime = 1099511628211ULL;
  for (char c : text) {
    hash ^= static_cast<unsigned char>(c);
    hash *= prime;
  }
  static const char hex[] = "0123456789abcdef";
  std::string out;
  out.reserve(16);
  for (int shift = 60; shift >= 0; shift -= 4) {
    out.push_back(hex[(hash >> shift) & 0xFULL]);
  }
  return out;
}

inline Artifact make_artifact(const std::string &payload,
                              const std::string &id = std::string()) {
  Artifact artifact;
  artifact.payload = payload;
  artifact.digest = fnv1a_hex(payload);
  artifact.bytes = payload.size();
  artifact.id = id.empty() ? artifact.digest : id;
  return artifact;
}

inline bool truthy_bool(const std::string &value) {
  return value == "true" || value == "1" || value == "yes";
}

inline bool falsy_bool(const std::string &value) {
  return value == "false" || value == "0" || value == "no";
}

inline gcp::Params normalize_params(const gcp::Params &input) {
  std::vector<gcp::Param> ordered;
  ordered.reserve(input.size());
  for (const gcp::Param &param : input) {
    if (param.key.empty()) {
      continue;
    }
    gcp::Param normalized;
    normalized.key = param.key;
    normalized.value = param.value;
    if (normalized.key == "confirm" || normalized.key == "wait" ||
        normalized.key == "deferred" || normalized.key == "await_async") {
      if (truthy_bool(normalized.value)) {
        normalized.value = "true";
      } else if (falsy_bool(normalized.value)) {
        normalized.value = "false";
      }
    }
    ordered.push_back(normalized);
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const gcp::Param &left, const gcp::Param &right) {
              if (left.key != right.key) {
                return left.key < right.key;
              }
              return left.value < right.value;
            });
  gcp::Params out;
  out.reserve(ordered.size());
  for (const gcp::Param &param : ordered) {
    if (!out.empty() && out.back().key == param.key) {
      out.back().value = param.value;
      continue;
    }
    out.push_back(param);
  }
  return out;
}

inline bool param_bool(const gcp::Params &params, const std::string &key,
                       bool fallback) {
  for (const gcp::Param &param : params) {
    if (param.key != key) {
      continue;
    }
    if (truthy_bool(param.value)) {
      return true;
    }
    if (falsy_bool(param.value)) {
      return false;
    }
    return fallback;
  }
  return fallback;
}

inline std::int64_t param_int(const gcp::Params &params, const std::string &key,
                              std::int64_t fallback) {
  for (const gcp::Param &param : params) {
    if (param.key == key) {
      try {
        return static_cast<std::int64_t>(std::stoll(param.value));
      } catch (...) {
        return fallback;
      }
    }
  }
  return fallback;
}

inline std::string sanitize_text(const std::string &text) {
  static const char *sensitive[] = {"token", "password", "secret",
                                    "credential", "api_key", "apikey"};
  std::string out;
  out.reserve(text.size());
  std::size_t index = 0;
  while (index < text.size()) {
    std::size_t end = index;
    while (end < text.size() && text[end] != ' ' && text[end] != '\n' &&
           text[end] != '\t' && text[end] != '&') {
      ++end;
    }
    const std::string token = text.substr(index, end - index);
    const std::size_t equals = token.find('=');
    if (equals == std::string::npos) {
      out += token;
    } else {
      const std::string key = token.substr(0, equals);
      std::string lowered;
      for (char c : key) {
        lowered.push_back(
            static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
      }
      bool masked = false;
      for (const char *needle : sensitive) {
        if (lowered.find(needle) != std::string::npos) {
          masked = true;
          break;
        }
      }
      out += key;
      out += masked ? "=[redacted]" : token.substr(equals);
    }
    if (end < text.size()) {
      out += text[end];
    }
    index = end + 1;
  }
  return out;
}

} // namespace control
} // namespace gda
