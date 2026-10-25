#ifndef GODOT_AUTOPILOT_CONSOLE_ENCODING_HPP
#define GODOT_AUTOPILOT_CONSOLE_ENCODING_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef GetObject
#undef GetObject
#endif
#endif

namespace godot_autopilot {
namespace util {

inline size_t utf8_sequence_length(unsigned char lead) {
  if ((lead & 0x80) == 0)
    return 1;
  if ((lead & 0xE0) == 0xC0)
    return 2;
  if ((lead & 0xF0) == 0xE0)
    return 3;
  if ((lead & 0xF8) == 0xF0)
    return 4;
  return 0;
}

inline bool utf8_sequence_is_valid(const unsigned char *bytes, size_t remaining,
                                   size_t length) {
  if (length == 0 || remaining < length)
    return false;
  if (length == 1)
    return (bytes[0] & 0x80) == 0;
  uint32_t codepoint = static_cast<uint32_t>(bytes[0] & (0x7F >> length));
  for (size_t i = 1; i < length; ++i) {
    if ((bytes[i] & 0xC0) != 0x80)
      return false;
    codepoint = (codepoint << 6) | static_cast<uint32_t>(bytes[i] & 0x3F);
  }
  static const uint32_t kMinimumCodePoint[5] = {0, 0, 0x80, 0x800, 0x10000};
  if (codepoint < kMinimumCodePoint[length])
    return false;
  if (codepoint > 0x10FFFF)
    return false;
  if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
    return false;
  return true;
}

inline bool is_valid_utf8(const std::string &text) {
  const unsigned char *bytes =
      reinterpret_cast<const unsigned char *>(text.data());
  size_t offset = 0;
  while (offset < text.size()) {
    const size_t length = utf8_sequence_length(bytes[offset]);
    if (!utf8_sequence_is_valid(bytes + offset, text.size() - offset, length))
      return false;
    offset += length;
  }
  return true;
}

inline std::string sanitize_utf8(const std::string &text) {
  std::string clean;
  clean.reserve(text.size());
  const unsigned char *bytes =
      reinterpret_cast<const unsigned char *>(text.data());
  size_t offset = 0;
  while (offset < text.size()) {
    const size_t length = utf8_sequence_length(bytes[offset]);
    if (utf8_sequence_is_valid(bytes + offset, text.size() - offset, length)) {
      clean.append(text, offset, length);
      offset += length;
      continue;
    }
    clean.push_back('?');
    ++offset;
  }
  return clean;
}

#ifdef _WIN32

inline bool decode_utf8_codepoint(const std::string &text, size_t offset,
                                  uint32_t &codepoint, size_t &length) {
  if (offset >= text.size())
    return false;
  const unsigned char *bytes =
      reinterpret_cast<const unsigned char *>(text.data());
  length = utf8_sequence_length(bytes[offset]);
  if (!utf8_sequence_is_valid(bytes + offset, text.size() - offset, length))
    return false;
  if (length == 1) {
    codepoint = bytes[offset];
    return true;
  }
  codepoint = static_cast<uint32_t>(bytes[offset] & (0x7F >> length));
  for (size_t i = 1; i < length; ++i)
    codepoint =
        (codepoint << 6) | static_cast<uint32_t>(bytes[offset + i] & 0x3F);
  return true;
}

inline bool is_cjk_ideograph(uint32_t codepoint) {
  return codepoint >= 0x4E00 && codepoint <= 0x9FFF;
}

inline bool contains_cjk_ideograph(const std::string &text) {
  size_t offset = 0;
  while (offset < text.size()) {
    uint32_t codepoint = 0;
    size_t length = 0;
    if (!decode_utf8_codepoint(text, offset, codepoint, length))
      return false;
    if (is_cjk_ideograph(codepoint))
      return true;
    offset += length;
  }
  return false;
}

inline bool contains_non_cjk_character(const std::string &text) {
  size_t offset = 0;
  while (offset < text.size()) {
    uint32_t codepoint = 0;
    size_t length = 0;
    if (!decode_utf8_codepoint(text, offset, codepoint, length))
      return false;
    if (!is_cjk_ideograph(codepoint))
      return true;
    offset += length;
  }
  return false;
}

inline bool convert_console_code_page(const std::string &text,
                                      unsigned int code_page,
                                      std::string &utf8_out) {
  if (text.empty()) {
    utf8_out.clear();
    return true;
  }
  const int byte_count = static_cast<int>(text.size());
  const int wide_length = MultiByteToWideChar(
      code_page, 0, text.data(), byte_count, nullptr, 0);
  if (wide_length <= 0)
    return false;
  std::wstring wide(static_cast<size_t>(wide_length), L'\0');
  if (MultiByteToWideChar(code_page, 0, text.data(), byte_count, wide.data(),
                          wide_length) <= 0)
    return false;
  const int utf8_length = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                              wide_length, nullptr, 0, nullptr,
                                              nullptr);
  if (utf8_length <= 0)
    return false;
  utf8_out.assign(static_cast<size_t>(utf8_length), '\0');
  if (WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_length, utf8_out.data(),
                          utf8_length, nullptr, nullptr) <= 0)
    return false;
  return true;
}

inline bool repair_acp_mojibake(const std::string &text,
                                std::string &utf8_out) {
  if (text.empty() || GetACP() == static_cast<UINT>(CP_UTF8))
    return false;
  const int byte_count = static_cast<int>(text.size());
  const int wide_length =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                          byte_count, nullptr, 0);
  if (wide_length <= 0)
    return false;
  std::wstring wide(static_cast<size_t>(wide_length), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                          byte_count, wide.data(), wide_length) <= 0)
    return false;
  const int acp_length = WideCharToMultiByte(
      CP_ACP, WC_NO_BEST_FIT_CHARS, wide.data(), wide_length, nullptr, 0,
      nullptr, nullptr);
  if (acp_length <= 0)
    return false;
  std::string acp_bytes(static_cast<size_t>(acp_length), '\0');
  BOOL used_default = FALSE;
  if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide.data(),
                          wide_length, acp_bytes.data(), acp_length, nullptr,
                          &used_default) <= 0)
    return false;
  if (used_default || acp_bytes == text || !is_valid_utf8(acp_bytes) ||
      !contains_cjk_ideograph(acp_bytes) ||
      !contains_non_cjk_character(text))
    return false;
  utf8_out = acp_bytes;
  return true;
}

#endif

inline std::string decode_console_output(const std::string &text) {
  if (text.empty())
    return text;
  if (is_valid_utf8(text)) {
#ifdef _WIN32
    std::string repaired;
    if (repair_acp_mojibake(text, repaired))
      return repaired;
#endif
    return text;
  }
#ifdef _WIN32
  std::string converted;
  const unsigned int oem_code_page = static_cast<unsigned int>(GetOEMCP());
  if (oem_code_page != 0 && oem_code_page != CP_UTF8 &&
      convert_console_code_page(text, oem_code_page, converted))
    return converted;
  if (convert_console_code_page(text, CP_ACP, converted))
    return converted;
#endif
  return sanitize_utf8(text);
}

} // namespace util
} // namespace godot_autopilot

#endif
