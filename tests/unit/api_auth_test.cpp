#include "core/api_auth.hpp"
#include "core/api_surface.hpp"
#include "core/api_token.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

using godot_autopilot::ApiAuth;
using godot_autopilot::api_surface::ApiReply;

namespace {

std::string make_temp_dir(const std::string &name) {
  static std::atomic<int> counter{0};
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("gda_api_auth_" + name + "_" +
       std::to_string(counter.fetch_add(1)));
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  return path.string();
}

void set_env(const char *name, const std::string &value) {
#ifdef _WIN32
  _putenv_s(name, value.c_str());
#else
  setenv(name, value.c_str(), 1);
#endif
}

std::unordered_map<std::string, std::string> bearer_headers(
    const std::string &token) {
  std::unordered_map<std::string, std::string> headers;
  headers["authorization"] = "Bearer " + token;
  return headers;
}

class ApiAuthFileTest : public ::testing::Test {
protected:
  void SetUp() override {
    saved_home_ = saved_env("GODOT_AUTOPILOT_HOME");
    saved_token_ = saved_env("GODOT_AUTOPILOT_API_TOKEN");
    home_ = make_temp_dir("home");
    set_env("GODOT_AUTOPILOT_HOME", home_);
    set_env("GODOT_AUTOPILOT_API_TOKEN", "");
    project_root_ = home_ + "/project";
    godot_autopilot::api_token::register_project_root_provider(
        [this]() { return project_root_; });
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(home_, ec);
    godot_autopilot::api_token::register_project_root_provider(nullptr);
    set_env("GODOT_AUTOPILOT_HOME", saved_home_);
    set_env("GODOT_AUTOPILOT_API_TOKEN", saved_token_);
  }

  static std::string saved_env(const char *name) {
    const char *value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string();
  }

  std::string saved_home_;
  std::string saved_token_;
  std::string home_;
  std::string project_root_;
};

} // namespace

TEST(ApiTokenTest, ParsesTheBearerHeader) {
  std::unordered_map<std::string, std::string> headers;
  headers["authorization"] = "Bearer abc123";
  std::string token;
  EXPECT_TRUE(godot_autopilot::api_token::parse_bearer_token(headers, token));
  EXPECT_EQ(token, "abc123");
}

TEST(ApiTokenTest, AcceptsTheLowercaseScheme) {
  std::unordered_map<std::string, std::string> headers;
  headers["Authorization"] = "bearer abc123";
  std::string token;
  EXPECT_TRUE(godot_autopilot::api_token::parse_bearer_token(headers, token));
  EXPECT_EQ(token, "abc123");
}

TEST(ApiTokenTest, RejectsTheMissingOrWrongAuthorizationHeader) {
  std::string token;
  std::unordered_map<std::string, std::string> missing;
  EXPECT_FALSE(
      godot_autopilot::api_token::parse_bearer_token(missing, token));
  std::unordered_map<std::string, std::string> basic;
  basic["authorization"] = "Basic abc123";
  EXPECT_FALSE(godot_autopilot::api_token::parse_bearer_token(basic, token));
  std::unordered_map<std::string, std::string> empty;
  empty["authorization"] = "Bearer   ";
  EXPECT_FALSE(godot_autopilot::api_token::parse_bearer_token(empty, token));
  std::unordered_map<std::string, std::string> scheme_only;
  scheme_only["authorization"] = "Bearer";
  EXPECT_FALSE(
      godot_autopilot::api_token::parse_bearer_token(scheme_only, token));
}

TEST(ApiTokenTest, ComparesTheTokensWithoutShortcuts) {
  EXPECT_TRUE(
      godot_autopilot::api_token::constant_time_equals("abc123", "abc123"));
  EXPECT_FALSE(
      godot_autopilot::api_token::constant_time_equals("abc123", "abc124"));
  EXPECT_FALSE(
      godot_autopilot::api_token::constant_time_equals("abc123", "abc12"));
  EXPECT_FALSE(
      godot_autopilot::api_token::constant_time_equals("abc123", ""));
  EXPECT_FALSE(
      godot_autopilot::api_token::constant_time_equals("", "abc123"));
}

TEST(ApiTokenTest, EncodesTheBytesAsLowercaseHex) {
  const std::vector<uint8_t> bytes = {0x00, 0x0f, 0xa5, 0xff};
  EXPECT_EQ(godot_autopilot::api_token::to_hex(bytes.data(), bytes.size()),
            "000fa5ff");
}

TEST(ApiTokenTest, GeneratesTheTokenFromTheInjectedRandomSource) {
  const std::vector<uint8_t> bytes = {0x00, 0x0f, 0xa5, 0xff};
  godot_autopilot::api_token::register_random_bytes(
      [bytes](uint8_t *buffer, size_t size) {
        for (size_t index = 0; index < size; ++index) {
          buffer[index] = bytes[index % bytes.size()];
        }
        return true;
      });
  const std::string token = godot_autopilot::api_token::generate_token();
  EXPECT_EQ(token.size(), godot_autopilot::api_token::kTokenBytes * 2);
  EXPECT_EQ(token.substr(0, 8), "000fa5ff");
  godot_autopilot::api_token::register_random_bytes(nullptr);
}

TEST_F(ApiAuthFileTest, IssuesAndVerifiesTheTokenFile) {
  const ApiAuth auth;
  const std::string token = auth.issue_token();
  ASSERT_FALSE(token.empty());
  EXPECT_EQ(token.size(), godot_autopilot::api_token::kTokenBytes * 2);
  EXPECT_EQ(auth.load_token(), token);
  EXPECT_TRUE(auth.verify(bearer_headers(token)));
}

TEST_F(ApiAuthFileTest, RejectsTheWrongAndTheMissingToken) {
  const ApiAuth auth;
  const std::string token = auth.issue_token();
  ASSERT_FALSE(token.empty());
  EXPECT_FALSE(auth.verify(bearer_headers(token + "0")));
  EXPECT_FALSE(auth.verify(bearer_headers("")));
  std::unordered_map<std::string, std::string> missing;
  EXPECT_FALSE(auth.verify(missing));
}

TEST_F(ApiAuthFileTest, InvalidatesTheOldTokenAfterTheRotation) {
  const ApiAuth auth;
  const std::string first = auth.issue_token();
  ASSERT_FALSE(first.empty());
  const std::string second = auth.issue_token();
  ASSERT_FALSE(second.empty());
  EXPECT_NE(first, second);
  EXPECT_FALSE(auth.verify(bearer_headers(first)));
  EXPECT_TRUE(auth.verify(bearer_headers(second)));
}

TEST_F(ApiAuthFileTest, KeepsTheTokenOutOfTheResponseBodies) {
  const ApiAuth auth;
  const std::string token = auth.issue_token();
  ASSERT_FALSE(token.empty());

  const ApiReply denied =
      godot_autopilot::api_surface::handle_request("GET", "/api/tools", {}, "");
  EXPECT_EQ(denied.status, 401);
  EXPECT_EQ(denied.body.Dump().find(token), std::string::npos);

  const ApiReply health =
      godot_autopilot::api_surface::handle_request("GET", "/api/health", {}, "");
  EXPECT_EQ(health.status, 200);
  EXPECT_EQ(health.body.Dump().find(token), std::string::npos);

  const ApiReply granted = godot_autopilot::api_surface::handle_request(
      "GET", "/api/tools", bearer_headers(token), "");
  EXPECT_EQ(granted.status, 200);
  EXPECT_EQ(granted.body.Dump().find(token), std::string::npos);
}
