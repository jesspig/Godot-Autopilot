#include "tools/domains/editor/editor_ui_actions_ops.hpp"
#include "tools/domains/input/input_click_ops.hpp"
#include "tools/domains/input_map/input_map_ops.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

using godot_autopilot::editor_ui_actions::parse_function_key;
using godot_autopilot::input_click_ops::parse_key_name;
using godot_autopilot::input_map_ops::resolve_function_key;
using godot_autopilot::input_map_ops::resolve_input_key;
using godot_autopilot::input_map_ops::resolve_key_name_code;

struct KeyCase {
  std::string name;
  int64_t code;
  bool authority_handles_name;
};

const std::vector<KeyCase> kSharedCases = {
    {"A", 65, true},
    {"z", 90, true},
    {"0", 48, true},
    {"9", 57, true},
    {"SPACE", 32, true},
    {"space", 32, true},
    {"KEY_SPACE", 32, true},
    {"ENTER", 4194309, true},
    {"KEY_ENTER", 4194309, true},
    {"RETURN", 4194309, true},
    {"ESCAPE", 4194305, true},
    {"KEY_ESCAPE", 4194305, true},
    {"SHIFT", 4194325, true},
    {"KEY_SHIFT", 4194325, true},
    {"CONTROL", 4194326, true},
    {"CTRL", 4194326, true},
    {"KEY_CTRL", 4194326, true},
    {"ALT", 4194328, true},
    {"TAB", 4194306, true},
    {"BACKSPACE", 4194308, true},
    {"DELETE", 4194312, true},
    {"LEFT", 4194319, true},
    {"RIGHT", 4194321, true},
    {"UP", 4194320, true},
    {"DOWN", 4194322, true},
    {"-", 45, false},
    {"=", 61, false},
    {"[", 91, false},
    {"]", 93, false},
    {";", 59, false},
    {"'", 39, false},
    {",", 44, false},
    {".", 46, false},
    {"/", 47, false},
    {"\\", 92, false},
    {"~", 126, false},
    {"KEY_F1", 4194332, true},
    {"KEY_F5", 4194336, true},
    {"KEY_F12", 4194343, true},
    {"4194309", 4194309, true},
    {"32", 32, true},
    {"KEY_80", 80, true},
};

} // namespace

TEST(InputKeyUnificationTest, ResolversShareOneAuthorityForCommonKeys) {
  for (const KeyCase &item : kSharedCases) {
    const godot::Key expected = static_cast<godot::Key>(item.code);
    if (item.authority_handles_name) {
      EXPECT_EQ(resolve_key_name_code(item.name), item.code) << item.name;
    }
    EXPECT_EQ(resolve_input_key(item.name), expected) << item.name;
    EXPECT_EQ(parse_key_name(item.name), expected) << item.name;
  }
}

TEST(InputKeyUnificationTest, FunctionKeysUseTheAuthoritativeBareTable) {
  for (int i = 1; i <= 12; ++i) {
    const std::string lower = "f" + std::to_string(i);
    const std::string key_prefixed = "KEY_F" + std::to_string(i);
    const int64_t code = resolve_key_name_code(key_prefixed);
    ASSERT_GT(code, 0) << key_prefixed;
    const godot::Key expected = static_cast<godot::Key>(code);
    EXPECT_EQ(parse_function_key(lower), expected) << lower;
    EXPECT_EQ(resolve_function_key(lower), expected) << lower;
  }
}

TEST(InputKeyUnificationTest, BareFunctionNamesStayOutOfGeneralParser) {
  EXPECT_EQ(resolve_input_key("f5"), godot::KEY_NONE);
  EXPECT_EQ(resolve_input_key("F5"), godot::KEY_NONE);
  EXPECT_EQ(parse_key_name("f5"), godot::KEY_NONE);
  EXPECT_EQ(resolve_function_key("f0"), godot::KEY_NONE);
  EXPECT_EQ(resolve_function_key("f13"), godot::KEY_NONE);
  EXPECT_EQ(resolve_function_key("fx"), godot::KEY_NONE);
}

TEST(InputKeyUnificationTest, UnknownAndMalformedNamesResolveToNone) {
  for (const std::string &bad : {"", "KEY_", "NOPE", "KEY_NOPE", "-1",
                                "0x50", "99999999", "123456789"}) {
    EXPECT_EQ(resolve_input_key(bad), godot::KEY_NONE) << bad;
    EXPECT_EQ(parse_key_name(bad), godot::KEY_NONE) << bad;
  }
}
