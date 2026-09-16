// keycode_alias_test.cpp — 键名 → 键码判定 (resolve_key_name_code) 的 L1 覆盖
//
// 判定实现在 src/tools/input_map_ops.cpp，被编辑器侧 add_input_map_action_event
// 与游戏侧运行时 src/runtime/game_bridge_input.cpp:parse_keycode 共用（后者只
// 做 JV → std::string 取值与 0..0xFFFFFF 区间复核）。本测试固定两侧的同一口径：
// 裸名 (P/0/SPACE)、KEY_ 前缀全名 (KEY_P/KEY_0/KEY_SPACE，大小写不敏感) 与数字码
// 字符串都解析到同一键码。函数不触碰任何 Godot 运行时状态，可纯单元测试。

#include "tools/input_map_ops.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace godot_autopilot {
namespace input_map_ops {

// 定义于 src/tools/input_map_ops.cpp（input_map_ops.hpp 只声明工具 handler）。
int64_t resolve_key_name_code(const std::string &name);

} // namespace input_map_ops
} // namespace godot_autopilot

using godot_autopilot::input_map_ops::resolve_key_name_code;

namespace {

constexpr int64_t kKeyCodeSpace = 32;
constexpr int64_t kKeyCode0 = 48;
constexpr int64_t kKeyCode9 = 57;
constexpr int64_t kKeyCodeP = 80;
constexpr int64_t kKeyCodeEscape = 4194305;
constexpr int64_t kKeyCodeEnter = 4194309;
constexpr int64_t kKeyCodeCtrl = 4194326;
constexpr int64_t kKeyCodeLeft = 4194319;
constexpr int64_t kKeyCodeNone = 0;

} // namespace

TEST(KeycodeAliasTest, BareLetterEqualsKeyPrefixedLetter) {
  EXPECT_EQ(kKeyCodeP, resolve_key_name_code("P"));
  EXPECT_EQ(kKeyCodeP, resolve_key_name_code("p"));
  EXPECT_EQ(kKeyCodeP, resolve_key_name_code("KEY_P"));
  EXPECT_EQ(kKeyCodeP, resolve_key_name_code("key_p"));
  EXPECT_EQ(kKeyCodeP, resolve_key_name_code("KeY_p"));
}

TEST(KeycodeAliasTest, BareDigitEqualsKeyPrefixedDigit) {
  EXPECT_EQ(kKeyCode0, resolve_key_name_code("0"));
  EXPECT_EQ(kKeyCode0, resolve_key_name_code("KEY_0"));
  EXPECT_EQ(kKeyCode9, resolve_key_name_code("9"));
  EXPECT_EQ(kKeyCode9, resolve_key_name_code("key_9"));
}

TEST(KeycodeAliasTest, SpecialKeysAcceptBothSpellings) {
  EXPECT_EQ(kKeyCodeSpace, resolve_key_name_code("SPACE"));
  EXPECT_EQ(kKeyCodeSpace, resolve_key_name_code("KEY_SPACE"));
  EXPECT_EQ(kKeyCodeSpace, resolve_key_name_code("space"));
  EXPECT_EQ(kKeyCodeLeft, resolve_key_name_code("LEFT"));
  EXPECT_EQ(kKeyCodeLeft, resolve_key_name_code("KEY_LEFT"));
  EXPECT_EQ(kKeyCodeEscape, resolve_key_name_code("ESCAPE"));
  EXPECT_EQ(kKeyCodeEscape, resolve_key_name_code("KEY_ESCAPE"));
  EXPECT_EQ(kKeyCodeEnter, resolve_key_name_code("KEY_ENTER"));
  EXPECT_EQ(kKeyCodeEnter, resolve_key_name_code("key_enter"));
  EXPECT_EQ(kKeyCodeEnter, resolve_key_name_code("RETURN")); // 引擎表裸别名
  EXPECT_EQ(kKeyCodeCtrl, resolve_key_name_code("KEY_CTRL"));
  EXPECT_EQ(kKeyCodeCtrl, resolve_key_name_code("CONTROL")); // 引擎表裸别名
}

TEST(KeycodeAliasTest, NumericStringsKeepWorking) {
  EXPECT_EQ(kKeyCodeEnter, resolve_key_name_code("4194309"));
  EXPECT_EQ(kKeyCodeP, resolve_key_name_code("80"));
  EXPECT_EQ(kKeyCodeP, resolve_key_name_code("KEY_80"));
  EXPECT_EQ(kKeyCodeSpace, resolve_key_name_code("32"));
}

TEST(KeycodeAliasTest, UnknownNamesResolveToNone) {
  EXPECT_EQ(kKeyCodeNone, resolve_key_name_code(""));
  EXPECT_EQ(kKeyCodeNone, resolve_key_name_code("KEY_"));
  EXPECT_EQ(kKeyCodeNone, resolve_key_name_code("NOPE"));
  EXPECT_EQ(kKeyCodeNone, resolve_key_name_code("KEY_NOPE"));
  EXPECT_EQ(kKeyCodeNone, resolve_key_name_code("-"));
  EXPECT_EQ(kKeyCodeNone, resolve_key_name_code("0x50"));
  EXPECT_EQ(kKeyCodeNone, resolve_key_name_code("99999999"));  // 超出 0xFFFFFF
  EXPECT_EQ(kKeyCodeNone, resolve_key_name_code("123456789")); // 9 位数字
}

TEST(KeycodeAliasTest, EveryLetterAndDigitResolvesToItsAsciiValue) {
  for (char c = 'A'; c <= 'Z'; ++c) {
    const std::string bare(1, c);
    const std::string lowered(1, static_cast<char>(c - 'A' + 'a'));
    const int64_t expected = static_cast<int64_t>(static_cast<unsigned char>(c));
    EXPECT_EQ(expected, resolve_key_name_code(bare)) << "bare " << c;
    EXPECT_EQ(expected, resolve_key_name_code(lowered))
        << "lowercase " << lowered;
    EXPECT_EQ(expected, resolve_key_name_code("KEY_" + bare))
        << "KEY_ prefix " << bare;
    EXPECT_EQ(expected, resolve_key_name_code("key_" + lowered))
        << "lowercase KEY_ prefix " << lowered;
  }
  for (char c = '0'; c <= '9'; ++c) {
    const std::string bare(1, c);
    const int64_t expected = static_cast<int64_t>(static_cast<unsigned char>(c));
    EXPECT_EQ(expected, resolve_key_name_code(bare)) << "bare " << c;
    EXPECT_EQ(expected, resolve_key_name_code("KEY_" + bare))
        << "KEY_ prefix " << bare;
    EXPECT_EQ(expected, resolve_key_name_code("key_" + bare))
        << "lowercase KEY_ prefix " << bare;
  }
}
