#pragma once
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace gsd_test {

std::string resolve_godot_path();

class GodotEditorFixture : public ::testing::Test {
public:
  struct Impl;

protected:
  GodotEditorFixture();
  ~GodotEditorFixture();
  void SetUp() override;
  void TearDown() override;

  int port() const;
  bool editor_alive() const;

  std::string capture_logs() const;
  void wait_for_exit(std::chrono::seconds timeout);

private:
  std::unique_ptr<Impl> impl_;
};

} // namespace gsd_test
