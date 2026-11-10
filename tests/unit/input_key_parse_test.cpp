#include <gtest/gtest.h>

#include <godot_cpp/classes/global_constants.hpp>
#include <string>
#include <tools/domains/input/input_click_ops.hpp>

namespace {

godot::Key key_of(const std::string &name) {
  return godot_autopilot::input_click_ops::parse_key_name(name);
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

TEST(InputKeyParseTest, CtrlMinusMainKeyResolves) {
  EXPECT_EQ(key_of("-"), godot::KEY_MINUS);
}

TEST(InputKeyParseTest, UnknownNamedKeyStillNone) {
  EXPECT_EQ(key_of("nope"), godot::KEY_NONE);
  EXPECT_EQ(key_of(""), godot::KEY_NONE);
}