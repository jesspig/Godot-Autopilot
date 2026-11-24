#include "util/project_path.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#else
#include <windows.h>
#include <winioctl.h>
#endif

#include <cstring>
#include <vector>

using godot_autopilot::util::containment_status_allows;
using godot_autopilot::util::normalize_project_path;
using godot_autopilot::util::ProjectPathResolveStatus;
using godot_autopilot::util::project_path_resolve_status;
using godot_autopilot::util::set_scheme_globalize_provider;

namespace {

#ifdef _WIN32
struct JunctionReparseData {
  uint32_t tag;
  uint16_t length;
  uint16_t reserved;
  uint16_t substitute_offset;
  uint16_t substitute_length;
  uint16_t print_offset;
  uint16_t print_length;
  wchar_t buffer[1];
};
#endif

bool create_directory_link(const std::filesystem::path &target,
                           const std::filesystem::path &link,
                           std::error_code &ec) {
#ifdef _WIN32
  std::filesystem::create_directory(link, ec);
  if (ec) {
    return false;
  }
  const std::wstring target_text = target.wstring();
  const std::wstring substitute_text = L"\\??\\" + target_text;
  const size_t substitute_bytes = substitute_text.size() * sizeof(wchar_t);
  const size_t print_bytes = target_text.size() * sizeof(wchar_t);
  const size_t path_bytes = substitute_bytes + sizeof(wchar_t) + print_bytes +
                            sizeof(wchar_t);
  const size_t header_bytes = offsetof(JunctionReparseData, buffer);
  std::vector<unsigned char> buffer(header_bytes + path_bytes, 0);
  auto *reparse = reinterpret_cast<JunctionReparseData *>(buffer.data());
  reparse->tag = IO_REPARSE_TAG_MOUNT_POINT;
  reparse->length = static_cast<uint16_t>(8 + path_bytes);
  reparse->substitute_offset = 0;
  reparse->substitute_length = static_cast<uint16_t>(substitute_bytes);
  reparse->print_offset =
      static_cast<uint16_t>(substitute_bytes + sizeof(wchar_t));
  reparse->print_length = static_cast<uint16_t>(print_bytes);
  std::memcpy(reparse->buffer, substitute_text.c_str(), substitute_bytes);
  std::memcpy(reparse->buffer + substitute_bytes / sizeof(wchar_t) + 1,
              target_text.c_str(), print_bytes);
  const HANDLE handle = CreateFileW(
      link.wstring().c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    ec = std::error_code(GetLastError(), std::system_category());
    std::error_code remove_ec;
    std::filesystem::remove(link, remove_ec);
    return false;
  }
  DWORD bytes_returned = 0;
  const BOOL updated = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT,
                                       buffer.data(),
                                       static_cast<DWORD>(buffer.size()), nullptr,
                                       0, &bytes_returned, nullptr);
  const DWORD error = updated ? ERROR_SUCCESS : GetLastError();
  CloseHandle(handle);
  if (!updated) {
    ec = std::error_code(error, std::system_category());
    std::error_code remove_ec;
    std::filesystem::remove(link, remove_ec);
    return false;
  }
  ec.clear();
  return true;
#else
  std::filesystem::create_directory_symlink(target, link, ec);
  return !ec;
#endif
}

class SchemePathContainmentTest : public ::testing::Test {
protected:
  void SetUp() override {
    base = std::filesystem::current_path() /
           ("gda_scheme_path_" +
            std::to_string(std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count()));
    root = base / "root";
    user = base / "user";
    outside = base / "outside";
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    ASSERT_FALSE(ec) << ec.message();
    std::filesystem::create_directories(user, ec);
    ASSERT_FALSE(ec) << ec.message();
    std::filesystem::create_directories(outside, ec);
    ASSERT_FALSE(ec) << ec.message();
    set_scheme_globalize_provider([this](const std::string &path) -> std::string {
      if (path.rfind("res://", 0) == 0) {
        return (root / path.substr(6)).generic_string();
      }
      if (path.rfind("user://", 0) == 0) {
        return (user / path.substr(7)).generic_string();
      }
      return {};
    });
  }

  void TearDown() override {
    set_scheme_globalize_provider({});
    std::error_code ec;
    std::filesystem::remove_all(base, ec);
  }

  std::filesystem::path base;
  std::filesystem::path root;
  std::filesystem::path user;
  std::filesystem::path outside;
};

TEST_F(SchemePathContainmentTest, OnlyInsideIsAllowed) {
  EXPECT_TRUE(containment_status_allows(ProjectPathResolveStatus::Inside));
  EXPECT_FALSE(containment_status_allows(ProjectPathResolveStatus::Outside));
  EXPECT_FALSE(containment_status_allows(ProjectPathResolveStatus::Unresolved));
}

TEST_F(SchemePathContainmentTest, RelativeContainmentInputsAreUnresolved) {
  EXPECT_EQ(project_path_resolve_status("relative/path", root.generic_string()),
            ProjectPathResolveStatus::Unresolved);
  EXPECT_EQ(project_path_resolve_status(root.generic_string(), "relative/root"),
            ProjectPathResolveStatus::Unresolved);
}

TEST_F(SchemePathContainmentTest, AcceptsInsideSchemeAndAbsolutePaths) {
  const auto scheme =
      normalize_project_path("res://themes/ui.tres", false, false);
  ASSERT_TRUE(scheme.valid());
  EXPECT_EQ(scheme.value, "res://themes/ui.tres");

  const auto absolute = normalize_project_path(
      (root / "themes/ui.tres").generic_string(), false, false);
  ASSERT_TRUE(absolute.valid());
  EXPECT_EQ(absolute.value, "res://themes/ui.tres");
}

TEST_F(SchemePathContainmentTest, RejectsSiblingWithSharedPrefix) {
  const auto result = normalize_project_path(
      (base / "root-sibling/file.tres").generic_string(), false, false);
  EXPECT_FALSE(result.valid());
}

TEST_F(SchemePathContainmentTest, RejectsSymlinkOutsideProject) {
  std::error_code ec;
  const std::filesystem::path link = root / "link_to_outside";
  create_directory_link(outside, link, ec);
  if (ec) {
    GTEST_SKIP() << "directory link creation not permitted on this platform";
  }

  EXPECT_EQ(project_path_resolve_status(link.generic_string(),
                                        root.generic_string()),
            ProjectPathResolveStatus::Outside);
  EXPECT_FALSE(
      normalize_project_path("res://link_to_outside/file.tres", false, false)
          .valid());
  EXPECT_FALSE(normalize_project_path((link / "file.tres").generic_string(),
                                      false, false)
                   .valid());
}

TEST_F(SchemePathContainmentTest, RejectsSymlinkOutsideUserNamespace) {
  std::error_code ec;
  const std::filesystem::path link = user / "link_to_outside";
  create_directory_link(outside, link, ec);
  if (ec) {
    GTEST_SKIP() << "directory link creation not permitted on this platform";
  }

  EXPECT_FALSE(normalize_project_path("user://link_to_outside/save.dat", true,
                                      false)
                   .valid());
}

TEST_F(SchemePathContainmentTest, AcceptsInsideUserPathOnlyWhenAllowed) {
  const auto allowed = normalize_project_path("user://save.dat", true, false);
  ASSERT_TRUE(allowed.valid());
  EXPECT_EQ(allowed.value, "user://save.dat");
  EXPECT_FALSE(normalize_project_path("user://save.dat", false, false).valid());
}

TEST_F(SchemePathContainmentTest, NamespaceRootHonorsAllowRoot) {
  ASSERT_TRUE(normalize_project_path("res://", false, true).valid());
  EXPECT_FALSE(normalize_project_path("res://", false, false).valid());
  const auto absolute =
      normalize_project_path(root.generic_string(), false, true);
  ASSERT_TRUE(absolute.valid());
  EXPECT_EQ(absolute.value, "res://");
}

TEST_F(SchemePathContainmentTest, LexicalEscapeIsRejected) {
  EXPECT_FALSE(
      normalize_project_path("res://tests/../../outside", false, false)
          .valid());
}

TEST_F(SchemePathContainmentTest, MissingProviderFailsClosed) {
  set_scheme_globalize_provider({});
  EXPECT_FALSE(
      normalize_project_path("res://themes/ui.tres", false, false).valid());
  EXPECT_FALSE(normalize_project_path((root / "themes/ui.tres").generic_string(),
                                      false, false)
                   .valid());
}

TEST_F(SchemePathContainmentTest, EmptyProviderResultFailsClosed) {
  set_scheme_globalize_provider([](const std::string &) -> std::string {
    return {};
  });
  EXPECT_FALSE(
      normalize_project_path("res://themes/ui.tres", false, false).valid());
  EXPECT_FALSE(normalize_project_path((root / "themes/ui.tres").generic_string(),
                                      false, false)
                   .valid());
}

TEST_F(SchemePathContainmentTest, ThrowingProviderFailsClosed) {
  set_scheme_globalize_provider([](const std::string &) -> std::string {
    throw std::runtime_error("globalize failed");
  });
  EXPECT_FALSE(
      normalize_project_path("res://themes/ui.tres", false, false).valid());
}

#ifndef _WIN32
TEST_F(SchemePathContainmentTest, SymlinkLoopIsUnresolvedAndRejected) {
  std::error_code ec;
  const std::filesystem::path loop = root / "loop";
  std::filesystem::create_directory_symlink(loop, loop, ec);
  if (ec) {
    GTEST_SKIP() << "symlink loop creation not permitted on this platform";
  }

  EXPECT_EQ(project_path_resolve_status(loop.generic_string(),
                                        root.generic_string()),
            ProjectPathResolveStatus::Unresolved);
  EXPECT_FALSE(normalize_project_path("res://loop", false, false).valid());
}
#endif

TEST_F(SchemePathContainmentTest, NonDirectoryAncestorIsRejected) {
  const std::filesystem::path blocker = root / "blocker";
  std::ofstream(blocker).close();
  EXPECT_EQ(project_path_resolve_status((blocker / "child.tres").generic_string(),
                                        root.generic_string()),
            ProjectPathResolveStatus::Unresolved);
  const auto result =
      normalize_project_path("res://blocker/child.tres", false, false);
  EXPECT_FALSE(result.valid());
  EXPECT_FALSE(normalize_project_path((blocker / "child.tres").generic_string(),
                                      false, false)
                   .valid());
}

#ifndef _WIN32
TEST_F(SchemePathContainmentTest, PermissionDeniedAncestorIsUnresolved) {
  if (::geteuid() == 0) {
    GTEST_SKIP() << "root can bypass directory permission checks";
  }
  const std::filesystem::path locked = root / "locked";
  std::error_code ec;
  std::filesystem::create_directory(locked, ec);
  ASSERT_FALSE(ec);
  std::filesystem::permissions(
      locked, std::filesystem::perms::none,
      std::filesystem::perm_options::replace, ec);
  if (ec) {
    GTEST_SKIP() << "permission changes are not available";
  }
  const auto status = project_path_resolve_status(
      (locked / "child.tres").generic_string(), root.generic_string());
  std::filesystem::permissions(locked, std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace, ec);
  EXPECT_EQ(status, ProjectPathResolveStatus::Unresolved);
  EXPECT_FALSE(
      normalize_project_path("res://locked/child.tres", false, false).valid());
}
#endif

} // namespace
