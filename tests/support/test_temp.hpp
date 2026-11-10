#ifndef GDA_TEST_SUPPORT_TEST_TEMP_HPP
#define GDA_TEST_SUPPORT_TEST_TEMP_HPP

#include <filesystem>
#include <string>

namespace gda_test {

inline std::filesystem::path test_temp_root() {
  const std::filesystem::path root =
      std::filesystem::current_path() / "gda_test_tmp";
  std::error_code ec;
  std::filesystem::create_directories(root, ec);
  return root;
}

} // namespace gda_test

#endif