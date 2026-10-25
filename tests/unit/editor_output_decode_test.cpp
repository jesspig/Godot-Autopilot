#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include "util/console_encoding.hpp"

namespace {

using godot_autopilot::util::decode_console_output;
using godot_autopilot::util::is_valid_utf8;

#ifdef _WIN32

std::string acp_predecoded_utf8(const std::string &utf8_text) {
  const int byte_count = static_cast<int>(utf8_text.size());
  const int wide_length = MultiByteToWideChar(CP_ACP, 0, utf8_text.data(),
                                              byte_count, nullptr, 0);
  if (wide_length <= 0)
    return std::string();
  std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
  if (MultiByteToWideChar(CP_ACP, 0, utf8_text.data(), byte_count, wide.data(),
                          wide_length) <= 0)
    return std::string();
  const int utf8_length = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                              wide_length, nullptr, 0, nullptr,
                                              nullptr);
  if (utf8_length <= 0)
    return std::string();
  std::string mojibake(static_cast<std::size_t>(utf8_length), '\0');
  if (WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_length,
                          mojibake.data(), utf8_length, nullptr, nullptr) <= 0)
    return std::string();
  return mojibake;
}

std::string acp_roundtrip_bytes(const std::string &utf8_text) {
  const int byte_count = static_cast<int>(utf8_text.size());
  const int wide_length = MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, utf8_text.data(), byte_count, nullptr, 0);
  if (wide_length <= 0)
    return std::string();
  std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8_text.data(),
                          byte_count, wide.data(), wide_length) <= 0)
    return std::string();
  const int acp_length =
      WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide.data(),
                          wide_length, nullptr, 0, nullptr, nullptr);
  if (acp_length <= 0)
    return std::string();
  std::string acp_bytes(static_cast<std::size_t>(acp_length), '\0');
  BOOL used_default = FALSE;
  if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide.data(),
                          wide_length, acp_bytes.data(), acp_length, nullptr,
                          &used_default) <= 0)
    return std::string();
  if (used_default)
    return std::string();
  return acp_bytes;
}

#endif

TEST(EditorOutputDecode, ValidUtf8ChinesePassesThrough) {
  const std::string text = "构建成功：dotnet build finished";
  EXPECT_TRUE(is_valid_utf8(text));
  EXPECT_EQ(decode_console_output(text), text);
}

TEST(EditorOutputDecode, AsciiAndEmptyPassThrough) {
  EXPECT_TRUE(is_valid_utf8(""));
  EXPECT_EQ(decode_console_output(""), "");
  EXPECT_EQ(decode_console_output("Build succeeded"), "Build succeeded");
}

TEST(EditorOutputDecode, GbkBytesBecomeReadableUtf8) {
  const std::string gbk =
      std::string("build ") + "\xB9\xB9\xBD\xA8\xB3\xC9\xB9\xA6";
  ASSERT_FALSE(is_valid_utf8(gbk));
  const std::string decoded = decode_console_output(gbk);
  EXPECT_FALSE(decoded.empty());
  EXPECT_TRUE(is_valid_utf8(decoded));
#ifdef _WIN32
  if (GetOEMCP() == 936)
    EXPECT_NE(decoded.find("构建成功"), std::string::npos);
#else
  EXPECT_EQ(decoded, "build ????????");
#endif
}

TEST(EditorOutputDecode, AcpMojibakeRoundTripIsRepaired) {
#ifndef _WIN32
  GTEST_SKIP() << "ACP round-trip repair is Windows-only";
#else
  if (GetACP() == static_cast<UINT>(CP_UTF8))
    GTEST_SKIP() << "UTF-8 ACP has no ACP predecode stage";
  const std::string expected = "正在";
  const std::string mojibake = acp_predecoded_utf8(expected);
  ASSERT_FALSE(mojibake.empty());
  ASSERT_TRUE(is_valid_utf8(mojibake));
  ASSERT_NE(mojibake, expected);
  if (GetACP() == 936u)
    EXPECT_EQ(mojibake, "姝ｅ湪");
  EXPECT_EQ(decode_console_output(mojibake), expected);
#endif
}

TEST(EditorOutputDecode, PureCjkTextIsNotRewrittenByAcpRoundTrip) {
#ifndef _WIN32
  GTEST_SKIP() << "ACP round-trip repair is Windows-only";
#else
  if (GetACP() != 936u)
    GTEST_SKIP() << "CP936 ACP round-trip of 山水 is required";
  const std::string text = "山水";
  const std::string acp_bytes = acp_roundtrip_bytes(text);
  ASSERT_EQ(acp_bytes, "\xC9\xBD\xCB\xAE");
  ASSERT_TRUE(is_valid_utf8(acp_bytes));
  ASSERT_NE(acp_bytes, text);
  const std::string decoded = decode_console_output(text);
  EXPECT_EQ(decoded, text);
  EXPECT_NE(decoded, acp_bytes);
#endif
}

TEST(EditorOutputDecode, InvalidBytesAreSanitizedWithoutCrash) {
  const std::string invalid = std::string("\xFF\xFE") + "\xE4\xB8";
  ASSERT_FALSE(is_valid_utf8(invalid));
  const std::string decoded = decode_console_output(invalid);
  EXPECT_TRUE(is_valid_utf8(decoded));
  EXPECT_FALSE(decoded.empty());
#ifndef _WIN32
  EXPECT_EQ(decoded, "????");
#endif
}

TEST(EditorOutputDecode, TruncatedSequenceDoesNotCrash) {
  const std::string truncated = std::string("ok") + "\xE4\xB8";
  ASSERT_FALSE(is_valid_utf8(truncated));
  const std::string decoded = decode_console_output(truncated);
  EXPECT_FALSE(decoded.empty());
  EXPECT_TRUE(is_valid_utf8(decoded));
#ifndef _WIN32
  EXPECT_EQ(decoded, "ok??");
#endif
}

TEST(EditorOutputDecode, StrictUtf8RejectsOverlongSurrogateAndOutOfRange) {
  EXPECT_FALSE(is_valid_utf8("\xC0\xAF"));
  EXPECT_FALSE(is_valid_utf8("\xE0\x80\xAF"));
  EXPECT_FALSE(is_valid_utf8("\xED\xA0\x80"));
  EXPECT_FALSE(is_valid_utf8("\xF4\x90\x80\x80"));
  EXPECT_FALSE(is_valid_utf8("\x80"));
  EXPECT_TRUE(is_valid_utf8("\xF0\x9F\x98\x80"));
}

} // namespace
