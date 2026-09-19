#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#endif

#include "godot_process.hpp"

#include <cstdlib>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

#ifndef PROJECT_ROOT
#error                                                                         \
    "PROJECT_ROOT 编译宏未定义（tests/CMakeLists.txt 已为 gda_test_runner 配置）"
#endif

namespace gda_test {

#ifdef _WIN32
namespace {

constexpr auto kImportTimeoutMs = 120000;
constexpr auto kExitGraceSeconds = 5;
constexpr auto kPollInterval = std::chrono::milliseconds(200);
constexpr auto kConnectProbeTimeout = std::chrono::seconds(5);
constexpr auto kIoTimeoutMs = 5000;

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

bool tcp_port_open(int port) {
  WsaGuard wsa;
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET)
    return false;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_port = htons(static_cast<u_short>(port));
  const bool open =
      connect(s, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == 0;
  closesocket(s);
  return open;
}

bool mcp_initialize_handshake(int port) {
  WsaGuard wsa;
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET)
    return false;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_port = htons(static_cast<u_short>(port));

  u_long nonblock = 1;
  ioctlsocket(s, FIONBIO, &nonblock);
  if (connect(s, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
    if (WSAGetLastError() != WSAEWOULDBLOCK) {
      closesocket(s);
      return false;
    }
    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(s, &wset);
    timeval tv{static_cast<long>(kConnectProbeTimeout.count()), 0};
    if (select(0, nullptr, &wset, nullptr, &tv) <= 0) {
      closesocket(s);
      return false;
    }
    int soerror = 0;
    int len = sizeof(soerror);
    if (getsockopt(s, SOL_SOCKET, SO_ERROR,
                    reinterpret_cast<char *>(&soerror), &len) != 0 ||
        soerror != 0) {
      closesocket(s);
      return false;
    }
  }
  u_long block = 0;
  ioctlsocket(s, FIONBIO, &block);

  const std::string body =
      R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"gda-test-runner","version":"0.1.0"}}})";
  const std::string req =
      "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) +
      "\r\nContent-Type: application/json\r\n"
      "Accept: application/json, text/event-stream\r\nContent-Length: " +
      std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
  if (send(s, req.data(), static_cast<int>(req.size()), 0) == SOCKET_ERROR) {
    closesocket(s);
    return false;
  }

  DWORD timeout_ms = kIoTimeoutMs;
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
             reinterpret_cast<const char *>(&timeout_ms), sizeof(timeout_ms));
  std::string resp;
  char buf[4096];
  while (true) {
    const int n = recv(s, buf, sizeof(buf), 0);
    if (n <= 0)
      break;
    resp.append(buf, n);
  }
  closesocket(s);
  return resp.find("serverInfo") != std::string::npos;
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
                                const std::string &project_path,
                                const std::string &engine_args) {
  return "\"" + godot_path + "\" " + engine_args + " --path \"" +
         project_path + "\"";
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

static void close_process(GodotProcess::Impl &impl);

void append_log(GodotProcess::Impl &impl, const std::string &line);

} // namespace

struct GodotProcess::Impl {
  std::string godot_path;
  std::string project_path;
  bool headless = true;
  int port = 0;
  HANDLE process = nullptr;
  HANDLE stdout_read = nullptr;
  HANDLE stderr_read = nullptr;
  std::thread stdout_thread;
  std::thread stderr_thread;
  std::mutex log_mutex;
  std::string stdout_buf;
  std::string stderr_buf;
  bool running = false;
  std::string last_error;

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

namespace {

void attach_process(GodotProcess::Impl &impl, PROCESS_INFORMATION &pi) {
  impl.process = pi.hProcess;
  CloseHandle(pi.hThread);
  impl.running = true;
  impl.stdout_thread =
      std::thread(pipe_read_loop, impl.stdout_read, std::ref(impl.log_mutex),
                  std::ref(impl.stdout_buf));
  impl.stderr_thread =
      std::thread(pipe_read_loop, impl.stderr_read, std::ref(impl.log_mutex),
                  std::ref(impl.stderr_buf));
}

void close_process(GodotProcess::Impl &impl) {
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

void append_log(GodotProcess::Impl &impl, const std::string &line) {
  std::lock_guard<std::mutex> lock(impl.log_mutex);
  impl.stdout_buf += line;
}

} // namespace

GodotProcess::GodotProcess(Options opts) : impl_(std::make_unique<Impl>()) {
  impl_->godot_path = opts.godot_path;
  impl_->project_path =
      opts.project_path.empty() ? std::string(PROJECT_ROOT) + "/Example"
                                : opts.project_path;
  impl_->headless = opts.headless;
  impl_->port = opts.port;
}

GodotProcess::~GodotProcess() { stop(); }

bool GodotProcess::start(std::chrono::seconds ready_timeout) {
  if (impl_->godot_path.empty()) {
    impl_->last_error =
        "godot_path 为空（必填，可先调用 resolve_godot_path()）";
    return false;
  }
  impl_->port = impl_->port == 0 ? pick_free_port() : impl_->port;
  if (impl_->port == 0) {
    impl_->last_error = "无法分配空闲端口";
    return false;
  }

  const std::string headless_flag = impl_->headless ? "--headless " : "";
  PROCESS_INFORMATION pi{};

  {
    const char *force_key = "GDA_FORCE_HEADLESS";
    char force_buf[64] = {0};
    const DWORD force_len =
        GetEnvironmentVariableA(force_key, force_buf, sizeof(force_buf));
    const bool had_force = force_len > 0 && force_len < sizeof(force_buf);
    const std::string force_value = had_force ? std::string(force_buf) : "";
    const auto restore_force_env = [&] {
      SetEnvironmentVariableA(force_key,
                              had_force ? force_value.c_str() : nullptr);
    };
    SetEnvironmentVariableA(force_key, "1");

    if (launch_process(build_godot_cmdline(impl_->godot_path,
                                           impl_->project_path,
                                           headless_flag + "--editor --import"),
                       pi, impl_->stdout_read, impl_->stderr_read)) {
      attach_process(*impl_, pi);
      if (WaitForSingleObject(impl_->process, kImportTimeoutMs) !=
          WAIT_OBJECT_0) {
        append_log(*impl_, "[gda] 警告：--import 未在 120s 内退出，继续尝试\n");
      }
      close_process(*impl_);
    } else {
      append_log(*impl_, "[gda] 警告：启动 --import 进程失败（错误码 " +
                             std::to_string(GetLastError()) +
                             "），继续尝试\n");
    }
    restore_force_env();
  }

  const char *port_key = "GODOT_AUTOPILOT_PORT";
  char old_buf[64] = {0};
  const DWORD old_len =
      GetEnvironmentVariableA(port_key, old_buf, sizeof(old_buf));
  const bool had_old = old_len > 0 && old_len < sizeof(old_buf);
  const std::string old_value = had_old ? std::string(old_buf) : "";
  const auto restore_port_env = [&] {
    SetEnvironmentVariableA(port_key, had_old ? old_value.c_str() : nullptr);
  };
  SetEnvironmentVariableA(port_key, std::to_string(impl_->port).c_str());

  const char *force_key = "GDA_FORCE_HEADLESS";
  char force_buf[64] = {0};
  const DWORD force_len =
      GetEnvironmentVariableA(force_key, force_buf, sizeof(force_buf));
  const bool had_force = force_len > 0 && force_len < sizeof(force_buf);
  const std::string force_value = had_force ? std::string(force_buf) : "";
  const auto restore_force_env = [&] {
    SetEnvironmentVariableA(force_key,
                            had_force ? force_value.c_str() : nullptr);
  };
  SetEnvironmentVariableA(force_key, "1");

  if (!launch_process(build_godot_cmdline(impl_->godot_path,
                                          impl_->project_path,
                                          headless_flag + "--editor"),
                      pi, impl_->stdout_read, impl_->stderr_read)) {
    restore_port_env();
    restore_force_env();
    impl_->last_error =
        "启动编辑器进程失败（错误码 " + std::to_string(GetLastError()) + "）";
    return false;
  }
  attach_process(*impl_, pi);
  restore_port_env();
  restore_force_env();

  const auto deadline = std::chrono::steady_clock::now() + ready_timeout;
  bool ready = false;
  while (std::chrono::steady_clock::now() < deadline) {
    if (tcp_port_open(impl_->port) &&
        mcp_initialize_handshake(impl_->port)) {
      ready = true;
      break;
    }
    std::this_thread::sleep_for(kPollInterval);
  }
  if (!ready) {
    impl_->last_error = "MCP 服务器未在 " +
                        std::to_string(ready_timeout.count()) +
                        "s 内就绪。编辑器日志：\n" + capture_logs();
    return false;
  }
  return true;
}

void GodotProcess::stop() {
  if (!impl_->process)
    return;
  if (WaitForSingleObject(impl_->process, 0) == WAIT_TIMEOUT) {
    run_taskkill(GetProcessId(impl_->process));
    if (WaitForSingleObject(impl_->process, kExitGraceSeconds * 1000) ==
        WAIT_TIMEOUT) {
      TerminateProcess(impl_->process, 1);
      WaitForSingleObject(impl_->process, 5000);
      append_log(*impl_,
                 "[gda] 编辑器未正常退出，已强杀（TerminateProcess）\n");
    }
  }
  DWORD exit_code = 0;
  GetExitCodeProcess(impl_->process, &exit_code);
  append_log(*impl_, "[gda] 编辑器退出码: " + std::to_string(exit_code) + "\n");
  close_process(*impl_);
}

bool GodotProcess::alive() const {
  if (!impl_->process)
    return false;
  return WaitForSingleObject(impl_->process, 0) == WAIT_TIMEOUT;
}

int GodotProcess::port() const { return impl_->port; }

std::string GodotProcess::capture_logs() {
  std::lock_guard<std::mutex> lock(impl_->log_mutex);
  std::string out = "--- stdout ---\n" + impl_->stdout_buf +
                    "\n--- stderr ---\n" + impl_->stderr_buf;
  impl_->stdout_buf.clear();
  impl_->stderr_buf.clear();
  return out;
}

std::string GodotProcess::last_error() const { return impl_->last_error; }

std::string GodotProcess::resolve_godot_path() {
  if (const char *env = std::getenv("GODOT_PATH")) {
    if (*env)
      return std::string(env);
  }
  return read_env_file_key("GODOT_PATH");
}

#else

struct GodotProcess::Impl {
  std::string last_error;
};

GodotProcess::GodotProcess(Options) : impl_(std::make_unique<Impl>()) {
  impl_->last_error = "Godot 进程管理仅支持 Windows（当前平台非 win32）";
}
GodotProcess::~GodotProcess() = default;

bool GodotProcess::start(std::chrono::seconds) { return false; }
void GodotProcess::stop() {}
bool GodotProcess::alive() const { return false; }
int GodotProcess::port() const { return 0; }
std::string GodotProcess::capture_logs() { return ""; }
std::string GodotProcess::last_error() const { return impl_->last_error; }
std::string GodotProcess::resolve_godot_path() { return ""; }
#endif

} // namespace gda_test
