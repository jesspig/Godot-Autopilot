#include <gtest/gtest.h>

#include <godot_cpp/classes/global_constants.hpp>
#include <string>
#include <tools/domains/editor/editor_ui_actions_ops.hpp>
#include <tools/domains/input/input_click_ops.hpp>

namespace {

godot::Key key_of(const std::string &name) {
  return godot_autopilot::input_click_ops::parse_key_name(name);
}

godot::Key function_key_of(const std::string &name) {
  return godot_autopilot::editor_ui_actions::parse_function_key(name);
}

} // namespace

TEST(InputKeyParseTest, PunctuationKeysMapToAsciiKeycodes) {
  EXPECT_EQ(key_of("-"), godot::KEY_MINUS);
  EXPECT_EQ(key_of("="), godot::KEY_EQUAL);
  EXPECT_EQ(key_of("["), godot::KEY_BRACKETLEFT);
  EXPECT_EQ(key_of("]"), godot::KEY_BRACKETRIGHT);
  EXPECT_EQ(key_of(";"), godot::KEY_SEMICOLON);
  EXPECT_EQ(key_of("'"), godot::KEY_APOSTROPHE);
  EXPECT_EQ(key_of(","), godot::KEY_COMMA);
  EXPECT_EQ(key_of("."), godot::KEY_PERIOD);
  EXPECT_EQ(key_of("/"), godot::KEY_SLASH);
  EXPECT_EQ(key_of("\\"), godot::KEY_BACKSLASH);
}

TEST(InputKeyParseTest, SpaceAndTildeArePrintablePassthrough) {
  EXPECT_EQ(key_of(" "), godot::KEY_SPACE);
  EXPECT_EQ(key_of("~"), static_cast<godot::Key>('~'));
}

TEST(InputKeyParseTest, LettersDigitsAndNamedKeysUnchanged) {
  EXPECT_EQ(key_of("a"), godot::KEY_A);
  EXPECT_EQ(key_of("A"), godot::KEY_A);
  EXPECT_EQ(key_of("z"), godot::KEY_Z);
  EXPECT_EQ(key_of("0"), godot::KEY_0);
  EXPECT_EQ(key_of("5"), godot::KEY_5);
  EXPECT_EQ(key_of("9"), godot::KEY_9);
  EXPECT_EQ(key_of("space"), godot::KEY_SPACE);
  EXPECT_EQ(key_of("ctrl"), godot::KEY_CTRL);
  EXPECT_EQ(key_of("escape"), godot::KEY_ESCAPE);
}

TEST(InputKeyParseTest, FunctionKeysParseF1ToF12) {
  EXPECT_EQ(function_key_of("f1"), godot::KEY_F1);
  EXPECT_EQ(function_key_of("f5"), godot::KEY_F5);
  EXPECT_EQ(function_key_of("f12"), godot::KEY_F12);
}

TEST(InputKeyParseTest, FunctionKeysRejectOutOfRangeAndMalformed) {
  EXPECT_EQ(function_key_of("f0"), godot::KEY_NONE);
  EXPECT_EQ(function_key_of("f13"), godot::KEY_NONE);
  EXPECT_EQ(function_key_of("fx"), godot::KEY_NONE);
  EXPECT_EQ(function_key_of("f"), godot::KEY_NONE);
  EXPECT_EQ(key_of("f5"), godot::KEY_NONE);
}

TEST(InputKeyParseTest, CtrlMinusMainKeyResolves) {
  EXPECT_EQ(key_of("-"), godot::KEY_MINUS);
  EXPECT_EQ(function_key_of("-"), godot::KEY_NONE);
}

TEST(InputKeyParseTest, UnknownNamedKeyStillNone) {
  EXPECT_EQ(key_of("nope"), godot::KEY_NONE);
  EXPECT_EQ(key_of(""), godot::KEY_NONE);
}