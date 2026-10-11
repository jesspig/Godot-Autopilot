#include <gtest/gtest.h>

#include <string>

static const char kMetadataPrefix[] = "metadata/";

static bool is_metadata_property(const std::string &name) {
  return name.rfind(kMetadataPrefix, 0) == 0;
}

static bool validate_metadata_key(const std::string &key, std::string &reason) {
  if (key.empty()) {
    reason = "metadata key must be non-empty";
    return false;
  }
  if (key.find("..") != std::string::npos) {
    reason = "metadata key must not contain '..'";
    return false;
  }
  for (char c : key) {
    if (c == ' ' || c == '\t' || c == ':' || c == '(' || c == ')' || c == '[' ||
        c == ']' || c == '"' || c == '\'' || c == '/') {
      reason = "metadata key contains an invalid character";
      return false;
    }
  }
  return true;
}

static std::string metadata_detail(const std::string &prop, const std::string &path,
                                   const std::string &reason) {
  std::string key = prop.substr(sizeof(kMetadataPrefix) - 1);
  return "invalid metadata key '" + key + "' in property '" + prop + "' on " + path + ": " +
         reason;
}

static const char kMetadataExpected[] =
    "a non-empty metadata key without spaces, :, (), [], / or ..";
static const char kMetadataAction[] =
    "pass 'metadata/<key>' with a plain key, e.g. 'metadata/my_key'";

TEST(MetadataKeyTest, PrefixedNameRoutesToMetadata) {
  EXPECT_TRUE(is_metadata_property("metadata/my_key"));
  EXPECT_TRUE(is_metadata_property("metadata/"));
}

TEST(MetadataKeyTest, PlainNameStaysProperty) {
  EXPECT_FALSE(is_metadata_property("position"));
  EXPECT_FALSE(is_metadata_property("metadata"));
  EXPECT_FALSE(is_metadata_property("Metadata/my_key"));
  EXPECT_FALSE(is_metadata_property("x/metadata/y"));
  EXPECT_FALSE(is_metadata_property(""));
}

TEST(MetadataKeyTest, PlainKeyValidates) {
  std::string reason;
  EXPECT_TRUE(validate_metadata_key("my_key", reason));
  EXPECT_TRUE(validate_metadata_key("key-1", reason));
  EXPECT_TRUE(validate_metadata_key("a.b", reason));
}

TEST(MetadataKeyTest, EmptyKeyRejected) {
  std::string reason;
  EXPECT_FALSE(validate_metadata_key("", reason));
  EXPECT_EQ("metadata key must be non-empty", reason);
}

TEST(MetadataKeyTest, DotDotRejected) {
  std::string reason;
  EXPECT_FALSE(validate_metadata_key("a..b", reason));
  EXPECT_EQ("metadata key must not contain '..'", reason);
  EXPECT_FALSE(validate_metadata_key("..", reason));
}

TEST(MetadataKeyTest, BannedCharactersRejected) {
  const char *banned[] = {"a b", "a\tb", "a:b", "a(b", "a)b",
                          "a[b", "a]b", "a\"b", "a'b", "a/b"};
  for (const char *key : banned) {
    std::string reason;
    EXPECT_FALSE(validate_metadata_key(key, reason)) << key;
    EXPECT_EQ("metadata key contains an invalid character", reason) << key;
  }
}

TEST(MetadataKeyTest, DetailMessageNamesKeyPropertyAndReason) {
  EXPECT_EQ("invalid metadata key 'a b' in property 'metadata/a b' on LabelNode: "
            "metadata key contains an invalid character",
            metadata_detail("metadata/a b", "LabelNode",
                            "metadata key contains an invalid character"));
}

TEST(MetadataKeyTest, ExpectedAndActionLiteralsMatchContract) {
  EXPECT_EQ("a non-empty metadata key without spaces, :, (), [], / or ..",
            std::string(kMetadataExpected));
  EXPECT_EQ("pass 'metadata/<key>' with a plain key, e.g. 'metadata/my_key'",
            std::string(kMetadataAction));
}
