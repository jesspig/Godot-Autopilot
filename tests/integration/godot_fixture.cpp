

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#endif

#include "godot_fixture.hpp"
#include "mcp_test_client.hpp"

#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

#ifndef PROJECT_ROOT
#error                                                                         \
    "PROJECT_ROOT 编译宏未定义（tests/CMakeLists.txt 已为 gsd_engine_tests 配置）"
#endif

namespace gsd_test {

#ifdef _WIN32
namespace {

constexpr auto kReadyTimeout = std::chrono::seconds(90);
constexpr auto kImportTimeoutMs = 120000;
constexpr auto kExitGraceSeconds = 5;
constexpr auto kPollInterval = std::chrono::milliseconds(200);
constexpr auto kConnectProbeTimeout = std::chrono::seconds(5);
const std::string kExamplePath = std::string(PROJECT_ROOT) + "/Example";

class WsaGuard {
public:
  WsaGuard() { WSAStartup(MAKEWORD(2, 2), &data_); }
  ~WsaGuard() { WSACleanup(); }

private:
  WSADATA data_;
};

std::string trim(const std::string &s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";
  const auto last = s.find_last_not_of(" \t\r\n");
  std::string out = s.substr(first, last - first + 1);
  if (out.size() >= 2 && out.front() == '"' && out.back() == '"') {
    out = out.substr(1, out.size() - 2);
  }
  return out;
}

std::string read_env_file_key(const std::string &key) {
  std::ifstream file(std::string(PROJECT_ROOT) + "/.env");
  if (!file)
    return "";
  std::string line;
  while (std::getline(file, line)) {
    const std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#')
      continue;
    const auto eq = trimmed.find('=');
    if (eq == std::string::npos)
      continue;
    if (trim(trimmed.substr(0, eq)) != key)
      continue;
    return trim(trimmed.substr(eq + 1));
  }
  return "";
}

int pick_free_port() {
  WsaGuard wsa;
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET)
    return 0;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_port = 0;
  if (bind(s, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
    closesocket(s);
    return 0;
  }
  sockaddr_in got{};
  int got_len = sizeof(got);
  if (getsockname(s, reinterpret_cast<sockaddr *>(&got), &got_len) != 0) {
    closesocket(s);
    return 0;
  }
  const int port = static_cast<int>(ntohs(got.sin_port));
  closesocket(s);
  return port;
}

void pipe_read_loop(HANDLE read_pipe, std::mutex &mutex, std::string &buffer) {
  char buf[4096];
  DWORD read = 0;
  while (ReadFile(read_pipe, buf, sizeof(buf), &read, nullptr) && read > 0) {
    std::lock_guard<std::mutex> lock(mutex);
    buffer.append(buf, read);
  }
}

bool create_pipe_pair(HANDLE &read_side, HANDLE &write_side) {
  SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  if (!CreatePipe(&read_side, &write_side, &sa, 0))
    return false;
  SetHandleInformation(read_side, HANDLE_FLAG_INHERIT, 0);
  return true;
}

bool launch_process(const std::string &cmdline, PROCESS_INFORMATION &pi,
                    HANDLE &stdout_read, HANDLE &stderr_read) {
  HANDLE stdout_write = nullptr;
  HANDLE stderr_write = nullptr;
  if (!create_pipe_pair(stdout_read, stdout_write))
    return false;
  if (!create_pipe_pair(stderr_read, stderr_write)) {
    CloseHandle(stdout_read);
    stdout_read = nullptr;
    return false;
  }

  std::vector<char> cmd(cmdline.begin(), cmdline.end());
  cmd.push_back('\0');

  STARTUPINFOA si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;

  HANDLE stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
  HANDLE stdin_fallback = INVALID_HANDLE_VALUE;
  if (stdin_handle == nullptr || stdin_handle == INVALID_HANDLE_VALUE) {
    stdin_fallback =
        CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr, OPEN_EXISTING, 0, nullptr);
    if (stdin_fallback != INVALID_HANDLE_VALUE) {
      SetHandleInformation(stdin_fallback, HANDLE_FLAG_INHERIT,
                           HANDLE_FLAG_INHERIT);
      stdin_handle = stdin_fallback;
    }
  }
  DWORD stdin_flags = 0;
  const bool stdin_inherit = GetHandleInformation(stdin_handle, &stdin_flags) &&
                             (stdin_flags & HANDLE_FLAG_INHERIT);
  if (!stdin_inherit)
    SetHandleInformation(stdin_handle, HANDLE_FLAG_INHERIT,
                         HANDLE_FLAG_INHERIT);
  si.hStdInput = stdin_handle;
  si.hStdOutput = stdout_write;
  si.hStdError = stderr_write;
  if (!stdin_inherit)
    SetHandleInformation(stdin_handle, HANDLE_FLAG_INHERIT, 0);

  const BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                 CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
  if (stdin_fallback != INVALID_HANDLE_VALUE)
    CloseHandle(stdin_fallback);

  CloseHandle(stdout_write);
  CloseHandle(stderr_write);
  return ok != FALSE;
}

std::string build_godot_cmdline(const std::string &godot_path,
                                const std::string &engine_args) {
  return "\"" + godot_path + "\" " + engine_args + " --path \"" + kExamplePath +
         "\"";
}

void run_taskkill(DWORD pid) {
  const std::string cmdline = "taskkill.exe /PID " + std::to_string(pid);
  std::vector<char> cmd(cmdline.begin(), cmdline.end());
  cmd.push_back('\0');
  STARTUPINFOA si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  if (CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                     CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    WaitForSingleObject(pi.hProcess, 2000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
}

static void close_process(GodotEditorFixture::Impl &impl);

void append_log(GodotEditorFixture::Impl &impl, const std::string &line);

} // namespace

struct GodotEditorFixture::Impl {
  int port = 0;
  std::string godot_path;
  HANDLE process = nullptr;
  HANDLE stdout_read = nullptr;
  HANDLE stderr_read = nullptr;
  std::thread stdout_thread;
  std::thread stderr_thread;
  std::mutex log_mutex;
  std::string stdout_buf;
  std::string stderr_buf;
  bool running = false;

  ~Impl() {
    if (running && process) {
      TerminateProcess(process, 1);
      WaitForSingleObject(process, 5000);
    }
    if (stdout_read) {
      CloseHandle(stdout_read);
      stdout_read = nullptr;
    }
    if (stderr_read) {
      CloseHandle(stderr_read);
      stderr_read = nullptr;
    }
    if (stdout_thread.joinable())
      stdout_thread.join();
    if (stderr_thread.joinable())
      stderr_thread.join();
    if (process) {
      CloseHandle(process);
      process = nullptr;
    }
    running = false;
  }
};

GodotEditorFixture::GodotEditorFixture() = default;
GodotEditorFixture::~GodotEditorFixture() = default;

namespace {

void close_process(GodotEditorFixture::Impl &impl) {
  if (!impl.process)
    return;
  if (WaitForSingleObject(impl.process, 0) == WAIT_TIMEOUT) {
    TerminateProcess(impl.process, 1);
    WaitForSingleObject(impl.process, 5000);
  }
  impl.running = false;
  if (impl.stdout_read) {
    CloseHandle(impl.stdout_read);
    impl.stdout_read = nullptr;
  }
  if (impl.stderr_read) {
    CloseHandle(impl.stderr_read);
    impl.stderr_read = nullptr;
  }
  if (impl.stdout_thread.joinable())
    impl.stdout_thread.join();
  if (impl.stderr_thread.joinable())
    impl.stderr_thread.join();
  CloseHandle(impl.process);
  impl.process = nullptr;
}

void append_log(GodotEditorFixture::Impl &impl, const std::string &line) {
  std::lock_guard<std::mutex> lock(impl.log_mutex);
  impl.stdout_buf += line;
}

} // namespace

std::string resolve_godot_path() {
  if (const char *env = std::getenv("GODOT_PATH")) {
    if (*env)
      return std::string(env);
  }
  return read_env_file_key("GODOT_PATH");
}

void GodotEditorFixture::SetUp() {
  impl_ = std::make_unique<Impl>();
  impl_->godot_path = resolve_godot_path();
  if (impl_->godot_path.empty()) {
    GTEST_SKIP() << "未找到 Godot 可执行文件（设置 GODOT_PATH 环境变量或仓库根 "
                    ".env 的 GODOT_PATH）";
  }
  impl_->port = pick_free_port();
  if (impl_->port == 0) {
    GTEST_SKIP() << "无法分配空闲端口";
  }

  const char *port_key = "GODOT_SELF_DRIVING_PORT";
  char old_buf[64] = {0};
  const DWORD old_len =
      GetEnvironmentVariableA(port_key, old_buf, sizeof(old_buf));
  const bool had_old = old_len > 0 && old_len < sizeof(old_buf);
  const std::string old_value = had_old ? std::string(old_buf) : "";
  const auto restore_port_env = [&] {
    SetEnvironmentVariableA(port_key, had_old ? old_value.c_str() : nullptr);
  };
  SetEnvironmentVariableA(port_key, std::to_string(impl_->port).c_str());

  PROCESS_INFORMATION pi{};
  if (!launch_process(build_godot_cmdline(impl_->godot_path,
                                          "--headless --editor --import"),
                      pi, impl_->stdout_read, impl_->stderr_read)) {
    restore_port_env();
    GTEST_SKIP() << "启动 --import 进程失败（错误码 " << GetLastError() << "）";
  }
  impl_->process = pi.hProcess;
  CloseHandle(pi.hThread);
  impl_->running = true;
  impl_->stdout_thread =
      std::thread(pipe_read_loop, impl_->stdout_read,
                  std::ref(impl_->log_mutex), std::ref(impl_->stdout_buf));
  impl_->stderr_thread =
      std::thread(pipe_read_loop, impl_->stderr_read,
                  std::ref(impl_->log_mutex), std::ref(impl_->stderr_buf));
  if (WaitForSingleObject(impl_->process, kImportTimeoutMs) != WAIT_OBJECT_0) {
    append_log(*impl_, "[fixture] 警告：--import 未在 120s 内退出，继续尝试\n");
  }
  close_process(*impl_);

  if (!launch_process(
          build_godot_cmdline(impl_->godot_path, "--headless --editor"), pi,
          impl_->stdout_read, impl_->stderr_read)) {
    restore_port_env();
    GTEST_SKIP() << "启动编辑器进程失败（错误码 " << GetLastError() << "）";
  }
  impl_->process = pi.hProcess;
  CloseHandle(pi.hThread);
  impl_->running = true;
  impl_->stdout_thread =
      std::thread(pipe_read_loop, impl_->stdout_read,
                  std::ref(impl_->log_mutex), std::ref(impl_->stdout_buf));
  impl_->stderr_thread =
      std::thread(pipe_read_loop, impl_->stderr_read,
                  std::ref(impl_->log_mutex), std::ref(impl_->stderr_buf));
  restore_port_env();

  const auto deadline = std::chrono::steady_clock::now() + kReadyTimeout;
  bool ready = false;
  while (std::chrono::steady_clock::now() < deadline) {
    McpTestClient probe(impl_->port);
    if (probe.connect(kConnectProbeTimeout)) {
      ready = true;
      break;
    }
    std::this_thread::sleep_for(kPollInterval);
  }
  if (!ready) {
    FAIL() << "MCP 服务器未在 " << kReadyTimeout.count()
           << "s 内就绪。编辑器日志：\n"
           << capture_logs();
  }
}

void GodotEditorFixture::TearDown() {
  if (impl_) {
    wait_for_exit(std::chrono::seconds(kExitGraceSeconds));
    impl_.reset();
  }
}

int GodotEditorFixture::port() const { return impl_ ? impl_->port : 0; }

bool GodotEditorFixture::editor_alive() const {
  if (!impl_ || !impl_->process)
    return false;
  return WaitForSingleObject(impl_->process, 0) == WAIT_TIMEOUT;
}

std::string GodotEditorFixture::capture_logs() const {
  std::lock_guard<std::mutex> lock(impl_->log_mutex);
  std::string out = "--- stdout ---\n" + impl_->stdout_buf +
                    "\n--- stderr ---\n" + impl_->stderr_buf;
  impl_->stdout_buf.clear();
  impl_->stderr_buf.clear();
  return out;
}

void GodotEditorFixture::wait_for_exit(std::chrono::seconds timeout) {
  if (!impl_ || !impl_->process)
    return;
  const auto wait_ms = static_cast<DWORD>(
      std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
  if (WaitForSingleObject(impl_->process, wait_ms) == WAIT_TIMEOUT) {

    run_taskkill(GetProcessId(impl_->process));
    WaitForSingleObject(impl_->process, kExitGraceSeconds * 1000);
    if (WaitForSingleObject(impl_->process, 0) == WAIT_TIMEOUT) {
      TerminateProcess(impl_->process, 1);
      WaitForSingleObject(impl_->process, 5000);
      append_log(*impl_,
                 "[fixture] 编辑器未正常退出，已强杀（TerminateProcess）\n");
    }
  }
  DWORD exit_code = 0;
  GetExitCodeProcess(impl_->process, &exit_code);
  append_log(*impl_,
             "[fixture] 编辑器退出码: " + std::to_string(exit_code) + "\n");
  close_process(*impl_);
}

#else

struct GodotEditorFixture::Impl {};

GodotEditorFixture::GodotEditorFixture() = default;
GodotEditorFixture::~GodotEditorFixture() = default;

std::string resolve_godot_path() { return ""; }

void GodotEditorFixture::SetUp() {
  GTEST_SKIP() << "L2 引擎内集成测试仅支持 Windows（当前平台非 win32）";
}
void GodotEditorFixture::TearDown() {}
int GodotEditorFixture::port() const { return 0; }
bool GodotEditorFixture::editor_alive() const { return false; }
std::string GodotEditorFixture::capture_logs() const { return ""; }
void GodotEditorFixture::wait_for_exit(std::chrono::seconds) {}
#endif

} // namespace gsd_test
