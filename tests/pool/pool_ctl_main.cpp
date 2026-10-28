#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "../integration/mcp_test_client.hpp"
#include "../runner/godot_process.hpp"

#include <mcp/JsonValue.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifndef PROJECT_ROOT
#error                                                                         \
    "PROJECT_ROOT 编译宏未定义（tests/CMakeLists.txt 已为 gda_pool_ctl 配置）"
#endif

namespace {

namespace fs = std::filesystem;

constexpr int kExitOk = 0;
constexpr int kExitFailed = 1;
constexpr int kExitError = 2;
constexpr auto kStatusProbeTimeout = std::chrono::seconds(5);
constexpr auto kReadyTimeout = std::chrono::seconds(90);
constexpr auto kStopGraceSeconds = std::chrono::seconds(3);
constexpr auto kStopForceSeconds = std::chrono::seconds(5);
constexpr auto kStopPollInterval = std::chrono::milliseconds(200);
constexpr char kAllowNone[] = "none";
constexpr char kAllowAll[] = "all";
constexpr char kHomeEnvKey[] = "GODOT_AUTOPILOT_HOME";
constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;
constexpr char kNoAlivePoolText[] = "no alive pool";

std::string default_state_dir() {
  return std::string(PROJECT_ROOT) + "/build/debug/tests/pool/state";
}

std::string default_pool_manifest() {
  return std::string(PROJECT_ROOT) + "/tests/pool/pools.json";
}

std::string pool_bed_path(const std::string &name) {
  return std::string(PROJECT_ROOT) + "/tests/testbed_" + name;
}

struct CliOptions {
  std::string command;
  std::string pool;
  bool has_pool = false;
  bool all = false;
  bool restart = false;
  std::string state_dir = default_state_dir();
};

enum class ParseResult { kOk, kError };

void print_usage(std::ostream &out) {
  out << "用法:\n"
      << "  gda_pool_ctl <start|stop|status|restart> [--pool <名>|--all]"
         " [--state-dir <目录>] [--restart]\n\n"
      << "子命令:\n"
      << "  start               启动池编辑器会话（床签名命中跳过 --import）\n"
      << "  stop                停止池进程并删除状态文件；无状态或进程已退出"
         "视为已停止\n"
      << "  status              枚举状态文件并报告各池存活状态\n"
      << "  restart             先停止再启动（等价于 start --restart）\n\n"
      << "参数:\n"
      << "  --pool <名>         只操作指定池（状态文件名去 .json 后缀）\n"
      << "  --all               操作全部池，与 --pool 互斥\n"
      << "  --state-dir <目录>  池状态文件目录（默认 " << default_state_dir()
      << "）\n"
      << "  --restart           仅 start：先停现有会话再启动\n\n"
      << "退出码:\n"
      << "  0  成功（stop 的已停止/残留状态清理同样为成功）\n"
      << "  1  stop 失败（进程 8 秒内未退出），或 --all 模式下存在失败的池\n"
      << "  2  参数或环境错误（restart 的停池失败同 start --restart）\n";
}

bool pool_name_is_valid(const std::string &name) {
  if (name.empty() || name == "." || name == "..") {
    return false;
  }
  return name.find('/') == std::string::npos &&
         name.find('\\') == std::string::npos &&
         name.find(':') == std::string::npos;
}

ParseResult parse_args(int argc, char **argv, CliOptions &opts,
                       std::string &error) {
  bool have_command = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--pool") {
      if (i + 1 >= argc) {
        error = "参数 --pool 缺少值";
        return ParseResult::kError;
      }
      opts.pool = argv[++i];
      opts.has_pool = true;
    } else if (arg == "--all") {
      opts.all = true;
    } else if (arg == "--state-dir") {
      if (i + 1 >= argc) {
        error = "参数 --state-dir 缺少值";
        return ParseResult::kError;
      }
      opts.state_dir = argv[++i];
    } else if (arg == "--restart") {
      opts.restart = true;
    } else if (!arg.empty() && arg.front() == '-') {
      error = "未知参数: " + arg;
      return ParseResult::kError;
    } else if (!have_command) {
      if (arg != "start" && arg != "stop" && arg != "status" &&
          arg != "restart") {
        error = "未知子命令: " + arg;
        return ParseResult::kError;
      }
      opts.command = arg;
      have_command = true;
    } else {
      error = "多余的位置参数: " + arg;
      return ParseResult::kError;
    }
  }
  if (!have_command) {
    error = "缺少子命令（start|stop|status|restart）";
    return ParseResult::kError;
  }
  if (opts.has_pool && opts.all) {
    error = "--pool 与 --all 互斥";
    return ParseResult::kError;
  }
  if (opts.has_pool && !pool_name_is_valid(opts.pool)) {
    error = "--pool 含非法路径字符: " + opts.pool;
    return ParseResult::kError;
  }
  if (opts.restart && opts.command != "start") {
    error = "--restart 仅支持 start 子命令";
    return ParseResult::kError;
  }
  return ParseResult::kOk;
}

#ifdef _WIN32
bool pid_alive(long long pid) {
  const HANDLE handle =
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE,
                  static_cast<DWORD>(pid));
  if (handle == nullptr) {
    return false;
  }
  const bool alive = WaitForSingleObject(handle, 0) == WAIT_TIMEOUT;
  CloseHandle(handle);
  return alive;
}
#else
bool pid_alive(long long) { return false; }
#endif

#ifdef _WIN32
void run_taskkill(long long pid, bool force) {
  std::string cmdline = "taskkill.exe /PID " + std::to_string(pid);
  if (force) {
    cmdline += " /F";
  }
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

bool wait_pid_exit(long long pid, std::chrono::seconds budget) {
  const auto deadline = std::chrono::steady_clock::now() + budget;
  while (pid_alive(pid)) {
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    std::this_thread::sleep_for(kStopPollInterval);
  }
  return true;
}

bool stop_pool_by_pid(long long pid, std::string &error) {
  if (!pid_alive(pid)) {
    return true;
  }
  run_taskkill(pid, false);
  if (!wait_pid_exit(pid, kStopGraceSeconds)) {
    run_taskkill(pid, true);
    if (!wait_pid_exit(pid, kStopForceSeconds)) {
      error = "池进程未在宽限期内退出: pid " + std::to_string(pid);
      return false;
    }
  }
  return true;
}
#else
bool stop_pool_by_pid(long long, std::string &) { return true; }
#endif

struct PoolState {
  std::string name;
  long long pid = 0;
  int port = 0;
};

bool read_pool_state(const fs::path &file, PoolState &state,
                     std::string &error) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    error = "无法打开状态文件: " + file.string();
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  mcp::JsonValue root;
  try {
    root = mcp::JsonValue::Parse(buffer.str());
  } catch (const std::exception &) {
    error = "状态文件不是合法 JSON: " + file.string();
    return false;
  }
  if (!root.IsObject()) {
    error = "状态文件顶层必须是 object: " + file.string();
    return false;
  }
  const mcp::JsonValue *pid = root.Find("pid");
  const mcp::JsonValue *port = root.Find("port");
  if (!pid || !pid->IsInt() || pid->GetInt() <= 0) {
    error = "状态文件 pid 字段缺失或非法: " + file.string();
    return false;
  }
  if (!port || !port->IsInt() || port->GetInt() <= 0 ||
      port->GetInt() > 65535) {
    error = "状态文件 port 字段缺失或非法: " + file.string();
    return false;
  }
  state.name = file.stem().string();
  state.pid = pid->GetInt();
  state.port = static_cast<int>(port->GetInt());
  return true;
}

const char *pool_verdict(const PoolState &state) {
  if (!pid_alive(state.pid)) {
    return "stale";
  }
  gda_test::McpTestClient client(state.port);
  return client.connect(kStatusProbeTimeout) ? "alive" : "dead";
}

int run_status(const CliOptions &opts) {
  std::error_code ec;
  fs::directory_iterator it(fs::path(opts.state_dir), ec);
  std::vector<fs::path> files;
  for (; !ec && it != fs::directory_iterator(); it.increment(ec)) {
    std::error_code entry_ec;
    if (!it->is_regular_file(entry_ec) || entry_ec) {
      continue;
    }
    const fs::path &path = it->path();
    if (path.extension() != ".json") {
      continue;
    }
    if (opts.has_pool && path.stem().string() != opts.pool) {
      continue;
    }
    files.push_back(path);
  }
  std::sort(files.begin(), files.end());
  if (files.empty()) {
    std::cout << kNoAlivePoolText << std::endl;
    return kExitOk;
  }
  for (const auto &file : files) {
    PoolState state;
    std::string error;
    if (!read_pool_state(file, state, error)) {
      std::cerr << "警告: " << error << std::endl;
      continue;
    }
    std::cout << state.name << " " << pool_verdict(state)
              << " port=" << state.port << std::endl;
  }
  return kExitOk;
}

struct PoolDef {
  std::string name;
  std::string allow;
  std::vector<std::string> cases;
};

const std::vector<std::string> &all_pool_names() {
  static const std::vector<std::string> names = {"deny", "main-1", "main-2",
                                                 "main-3", "main-4"};
  return names;
}

bool pool_name_known(const std::string &name) {
  const std::vector<std::string> &names = all_pool_names();
  return std::find(names.begin(), names.end(), name) != names.end();
}

std::string pool_allow_for(const std::string &name) {
  return name == "deny" ? std::string(kAllowNone) : std::string(kAllowAll);
}

bool load_pool_defs(const std::string &path, std::vector<PoolDef> &defs,
                    std::string &error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "无法打开池定义文件: " + path;
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  mcp::JsonValue root;
  try {
    root = mcp::JsonValue::Parse(buffer.str());
  } catch (const std::exception &e) {
    error = "池定义不是合法 JSON: " + path + "（" + e.what() + "）";
    return false;
  }
  if (!root.IsObject()) {
    error = "池定义顶层必须是 object: " + path;
    return false;
  }
  const mcp::JsonValue *version = root.Find("version");
  if (!version || !version->IsInt() || version->GetInt() != 1) {
    error = "池定义 version 字段缺失或非 1: " + path;
    return false;
  }
  const mcp::JsonValue *mode = root.Find("mode");
  if (!mode || !mode->IsString() ||
      (mode->GetString() != "pool" && mode->GetString() != "per-test")) {
    error = "池定义 mode 字段缺失或非 pool|per-test: " + path;
    return false;
  }
  const mcp::JsonValue *pools = root.Find("pools");
  if (!pools || !pools->IsArray() || pools->Size() == 0) {
    error = "池定义 pools 字段缺失或为空数组: " + path;
    return false;
  }
  for (size_t i = 0; i < pools->Size(); ++i) {
    const mcp::JsonValue &entry = (*pools)[i];
    const std::string at = "pools[" + std::to_string(i) + "]";
    if (!entry.IsObject()) {
      error = "池定义 " + at + " 必须是 object: " + path;
      return false;
    }
    PoolDef def;
    const mcp::JsonValue *name = entry.Find("name");
    if (!name || !name->IsString() || !pool_name_known(name->GetString())) {
      error = "池定义 " + at +
              ".name 缺失或不在 deny|main-1..main-4 枚举内: " + path;
      return false;
    }
    def.name = name->GetString();
    const mcp::JsonValue *allow = entry.Find("allow");
    if (!allow || !allow->IsString() ||
        allow->GetString() != pool_allow_for(def.name)) {
      error = "池定义 " + at + ".allow 缺失或与池名不符（" + def.name +
              " 应为 " + pool_allow_for(def.name) + "）: " + path;
      return false;
    }
    def.allow = allow->GetString();
    const mcp::JsonValue *cases = entry.Find("cases");
    if (!cases || !cases->IsArray()) {
      error = "池定义 " + at + ".cases 缺失或非数组: " + path;
      return false;
    }
    for (size_t c = 0; c < cases->Size(); ++c) {
      const mcp::JsonValue &item = (*cases)[c];
      if (!item.IsString() || item.GetString().empty()) {
        error = "池定义 " + at + ".cases[" + std::to_string(c) +
                "] 必须是非空字符串: " + path;
        return false;
      }
      def.cases.push_back(item.GetString());
    }
    for (const PoolDef &existing : defs) {
      if (existing.name == def.name) {
        error = "池定义 " + at + ".name 重复: " + def.name;
        return false;
      }
    }
    defs.push_back(std::move(def));
  }
  return true;
}

const PoolDef *find_pool_def(const std::vector<PoolDef> &defs,
                             const std::string &name) {
  for (const PoolDef &def : defs) {
    if (def.name == name) {
      return &def;
    }
  }
  return nullptr;
}

std::string json_escape(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    if (c == '\\' || c == '"') {
      out += '\\';
    }
    out += c;
  }
  return out;
}

std::string local_time_iso8601() {
  const std::time_t now = std::time(nullptr);
  std::tm parts{};
#ifdef _WIN32
  if (localtime_s(&parts, &now) != 0) {
    parts = std::tm{};
  }
#else
  const std::tm *decoded = std::localtime(&now);
  if (decoded != nullptr) {
    parts = *decoded;
  }
#endif
  std::ostringstream out;
  out << std::put_time(&parts, "%Y-%m-%dT%H:%M:%S%z");
  return out.str();
}

bool bed_is_ready(const std::string &bed) {
  const fs::path bed_dir(bed);
  return fs::is_regular_file(bed_dir / "project.godot") &&
         fs::is_regular_file(bed_dir / "addons" / "godot-autopilot" /
                             "godot-autopilot.gdextension");
}

uint64_t fnv1a_update(uint64_t hash, const void *data, size_t size) {
  const unsigned char *bytes = static_cast<const unsigned char *>(data);
  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= kFnvPrime;
  }
  return hash;
}

uint64_t fnv1a_update_text(uint64_t hash, const std::string &text) {
  return fnv1a_update(hash, text.data(), text.size());
}

bool mix_file_entry(uint64_t &hash, const fs::path &base, const fs::path &file,
                    std::string &error) {
  hash = fnv1a_update_text(hash,
                           file.lexically_relative(base).generic_string());
  std::error_code ec;
  const uintmax_t size = fs::file_size(file, ec);
  if (ec) {
    error = "无法读取文件大小: " + file.string();
    return false;
  }
  const uint64_t length = static_cast<uint64_t>(size);
  hash = fnv1a_update(hash, &length, sizeof(length));
  return true;
}

bool compute_bed_signature(const std::string &bed,
                           const std::string &godot_path,
                           std::string &signature, std::string &error) {
  const fs::path bed_dir(bed);
  uint64_t hash = kFnvOffsetBasis;
  std::vector<std::string> addon_paths;
  std::error_code ec;
  fs::recursive_directory_iterator it(bed_dir / "addons",
                                      fs::directory_options::skip_permission_denied,
                                      ec),
      end;
  for (; !ec && it != end; it.increment(ec)) {
    std::error_code entry_ec;
    if (it->is_regular_file(entry_ec) && !entry_ec) {
      addon_paths.push_back(
          it->path().lexically_relative(bed_dir).generic_string());
    }
  }
  if (ec) {
    error = "无法遍历床 addons 目录: " + (bed_dir / "addons").string();
    return false;
  }
  std::sort(addon_paths.begin(), addon_paths.end());
  for (const std::string &rel : addon_paths) {
    hash = fnv1a_update_text(hash, rel);
    if (!mix_file_entry(hash, bed_dir, bed_dir / fs::path(rel), error)) {
      return false;
    }
  }
  if (!mix_file_entry(hash, bed_dir, bed_dir / "project.godot", error)) {
    return false;
  }
  const fs::path version_file = fs::path(std::string(PROJECT_ROOT)) / "VERSION";
  std::ifstream in(version_file, std::ios::binary);
  if (!in) {
    error = "无法读取 VERSION 文件: " + version_file.string();
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  hash = fnv1a_update_text(hash, buffer.str());
  hash = fnv1a_update_text(hash, godot_path);
  std::error_code time_ec;
  const fs::file_time_type godot_time =
      fs::last_write_time(fs::path(godot_path), time_ec);
  if (time_ec) {
    error = "无法读取 Godot 可执行文件修改时间: " + godot_path;
    return false;
  }
  const long long godot_ticks = godot_time.time_since_epoch().count();
  hash = fnv1a_update(hash, &godot_ticks, sizeof(godot_ticks));
  std::ostringstream out;
  out << std::hex << std::setw(16) << std::setfill('0') << hash;
  signature = out.str();
  return true;
}

std::string read_import_stamp(const std::string &bed) {
  std::ifstream in(fs::path(bed) / ".godot" / "gda_import_stamp");
  if (!in) {
    return std::string();
  }
  std::string line;
  if (!std::getline(in, line)) {
    return std::string();
  }
  const auto first = line.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return std::string();
  }
  const auto last = line.find_last_not_of(" \t\r\n");
  return line.substr(first, last - first + 1);
}

bool write_import_stamp(const std::string &bed, const std::string &signature,
                        std::string &error) {
  const fs::path stamp = fs::path(bed) / ".godot" / "gda_import_stamp";
  std::error_code ec;
  fs::create_directories(stamp.parent_path(), ec);
  if (ec && !fs::is_directory(stamp.parent_path())) {
    error = "无法创建目录: " + stamp.parent_path().string();
    return false;
  }
  std::ofstream out(stamp, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "无法写入 import 标记: " + stamp.string();
    return false;
  }
  out << signature << "\n";
  out.flush();
  if (!out) {
    error = "import 标记写入失败: " + stamp.string();
    return false;
  }
  return true;
}

struct PoolStateRecord {
  long long pid = 0;
  int port = 0;
  std::string bed;
  std::string allow;
  std::string signature;
  std::string started_at;
};

bool write_pool_state(const fs::path &file, const PoolStateRecord &record,
                      std::string &error) {
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  if (ec && !fs::is_directory(file.parent_path())) {
    error = "无法创建状态目录: " + file.parent_path().string();
    return false;
  }
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "无法写入状态文件: " + file.string();
    return false;
  }
  out << "{\n"
      << "  \"pid\": " << record.pid << ",\n"
      << "  \"port\": " << record.port << ",\n"
      << "  \"bed\": \"" << json_escape(record.bed) << "\",\n"
      << "  \"allow\": \"" << json_escape(record.allow) << "\",\n"
      << "  \"signature\": \"" << json_escape(record.signature) << "\",\n"
      << "  \"started_at\": \"" << json_escape(record.started_at)
      << "\"\n"
      << "}\n";
  out.flush();
  if (!out) {
    error = "状态文件写入失败: " + file.string();
    return false;
  }
  return true;
}

void remove_file_quietly(const fs::path &file) {
  std::error_code ec;
  fs::remove(file, ec);
}

fs::path pool_state_file(const std::string &state_dir, const std::string &name) {
  return fs::path(state_dir) / (name + ".json");
}

enum class StopOutcome {
  kStopped,
  kNoState,
  kStaleState,
  kInvalidState,
  kFailed
};

struct StopResult {
  StopOutcome outcome = StopOutcome::kFailed;
  std::string error;
  long long pid = 0;
};

bool remove_state_file(const fs::path &file, std::string &error) {
  std::error_code ec;
  fs::remove(file, ec);
  if (ec) {
    error = "无法删除状态文件: " + file.string();
    return false;
  }
  return true;
}

StopResult stop_pool(const std::string &name, const std::string &state_dir) {
  const fs::path file = pool_state_file(state_dir, name);
  std::error_code ec;
  const bool exists = fs::exists(file, ec);
  if (ec) {
    return StopResult{StopOutcome::kFailed,
                      "无法检查状态文件: " + file.string()};
  }
  if (!exists) {
    return StopResult{StopOutcome::kNoState};
  }
  PoolState state;
  std::string error;
  if (!read_pool_state(file, state, error)) {
    const std::string invalid = error;
    if (!remove_state_file(file, error)) {
      return StopResult{StopOutcome::kFailed, error};
    }
    return StopResult{StopOutcome::kInvalidState, invalid};
  }
  if (!pid_alive(state.pid)) {
    if (!remove_state_file(file, error)) {
      return StopResult{StopOutcome::kFailed, error, state.pid};
    }
    return StopResult{StopOutcome::kStaleState, std::string(), state.pid};
  }
  if (!stop_pool_by_pid(state.pid, error)) {
    return StopResult{StopOutcome::kFailed, error, state.pid};
  }
  if (!remove_state_file(file, error)) {
    return StopResult{StopOutcome::kFailed, error, state.pid};
  }
  return StopResult{StopOutcome::kStopped, std::string(), state.pid};
}

int run_stop_one(const std::string &name, const CliOptions &opts) {
  const StopResult stopped = stop_pool(name, opts.state_dir);
  if (stopped.outcome == StopOutcome::kFailed) {
    std::cerr << "错误: [" << name << "] " << stopped.error << std::endl;
    return kExitFailed;
  }
  if (stopped.outcome == StopOutcome::kInvalidState) {
    std::cerr << "警告: " << stopped.error << "，按残留状态清理" << std::endl;
  }
  std::cout << "[" << name << "] "
            << (stopped.outcome == StopOutcome::kStopped ? "stopped"
                                                         : "already stopped")
            << std::endl;
  return kExitOk;
}

int run_stop_all(const CliOptions &opts) {
  int failed = 0;
  for (const std::string &name : all_pool_names()) {
    if (run_stop_one(name, opts) != kExitOk) {
      ++failed;
    }
  }
  if (failed > 0) {
    std::cerr << "错误: " << failed << " 个池停止失败" << std::endl;
    return kExitFailed;
  }
  return kExitOk;
}

int run_start_one(const std::string &name, const PoolDef &def,
                  const CliOptions &opts) {
  std::string error;
  const fs::path state_file = pool_state_file(opts.state_dir, name);
  if (opts.restart) {
    const StopResult stopped = stop_pool(name, opts.state_dir);
    if (stopped.outcome == StopOutcome::kFailed) {
      std::cerr << "错误: [" << name << "] " << stopped.error << std::endl;
      return kExitError;
    }
    if (stopped.outcome == StopOutcome::kStaleState) {
      std::cerr << "警告: [" << name << "] 状态文件残留但 pid " << stopped.pid
                << " 已退出，清理后重新启动" << std::endl;
    } else if (stopped.outcome == StopOutcome::kInvalidState) {
      std::cerr << "警告: " << stopped.error << "，按残留状态清理" << std::endl;
    }
  } else if (fs::exists(state_file)) {
    PoolState state;
    if (!read_pool_state(state_file, state, error)) {
      std::cerr << "警告: " << error << "，按残留状态清理" << std::endl;
      remove_file_quietly(state_file);
    } else if (pid_alive(state.pid)) {
      std::cerr << "警告: [" << name << "] 池编辑器已在运行 (pid "
                << state.pid << ")，复用现有会话" << std::endl;
      std::ostringstream out;
      out << "[" << name << "] already running, reused";
      std::cout << out.str() << std::endl;
      return kExitOk;
    } else {
      std::cerr << "警告: [" << name << "] 状态文件残留但 pid " << state.pid
                << " 已退出，清理后重新启动" << std::endl;
      remove_file_quietly(state_file);
    }
  }

  const std::string bed = pool_bed_path(name);
  if (!bed_is_ready(bed)) {
    std::cerr << "错误: L2 测试床未就绪: " << bed
              << " 缺少 project.godot 或 addons/godot-autopilot/"
                 "godot-autopilot.gdextension，请先运行 uv run main.py build "
                 "与 uv run main.py deploy" << std::endl;
    return kExitError;
  }
  const std::string godot_path = gda_test::GodotProcess::resolve_godot_path();
  if (godot_path.empty()) {
    std::cerr << "错误: 未找到 Godot 可执行文件。请设置 GODOT_PATH 环境"
                 "变量，或参照 .env.template 在仓库根 .env 中配置 "
                 "GODOT_PATH" << std::endl;
    return kExitError;
  }

  std::string signature;
  if (!compute_bed_signature(bed, godot_path, signature, error)) {
    std::cerr << "错误: [" << name << "] 计算床签名失败: " << error
              << std::endl;
    return kExitError;
  }
  const bool run_import = read_import_stamp(bed) != signature;

  const fs::path home_dir = fs::path(opts.state_dir) / "home" / name;
  std::error_code home_ec;
  fs::create_directories(home_dir, home_ec);
  if (home_ec && !fs::is_directory(home_dir)) {
    std::cerr << "错误: [" << name << "] 无法创建池 home 目录: "
              << home_dir.string() << std::endl;
    return kExitError;
  }
#ifdef _WIN32
  char home_old[1024] = {0};
  const DWORD home_old_len =
      GetEnvironmentVariableA(kHomeEnvKey, home_old, sizeof(home_old));
  const bool had_home = home_old_len > 0 && home_old_len < sizeof(home_old);
  const std::string home_old_value = had_home ? std::string(home_old) : "";
  SetEnvironmentVariableA(kHomeEnvKey, home_dir.string().c_str());
#endif

  gda_test::GodotProcess::Options options;
  options.godot_path = godot_path;
  options.project_path = bed;
  options.port = 0;
  options.headless = true;
  options.allow_list = def.allow;
  options.run_import = run_import;
  options.console_log_path = (home_dir / "editor-console.log").string();
  gda_test::GodotProcess proc(options);
  const bool started = proc.start(kReadyTimeout);
#ifdef _WIN32
  SetEnvironmentVariableA(kHomeEnvKey,
                          had_home ? home_old_value.c_str() : nullptr);
#endif
  if (!started) {
    std::cerr << "错误: [" << name << "] 池编辑器启动失败: "
              << proc.last_error() << std::endl;
    remove_file_quietly(state_file);
    return kExitError;
  }
  if (run_import) {
    if (!write_import_stamp(bed, signature, error)) {
      std::cerr << "警告: [" << name << "] " << error
                << "，下次启动将重新 --import" << std::endl;
    }
  }
  PoolStateRecord record;
  record.pid = proc.pid();
  record.port = proc.port();
  record.bed = fs::path(bed).generic_string();
  record.allow = def.allow;
  record.signature = signature;
  record.started_at = local_time_iso8601();
  if (record.pid <= 0) {
    std::cerr << "错误: [" << name << "] 无法获取池编辑器进程号，停止池编辑器"
              << std::endl;
    proc.stop();
    return kExitError;
  }
  if (!write_pool_state(state_file, record, error)) {
    std::cerr << "错误: [" << name << "] " << error << "，停止池编辑器"
              << std::endl;
    proc.stop();
    return kExitError;
  }
  std::ostringstream out;
  out << "[" << name << "] ready on port " << record.port << " ("
      << (run_import ? "imported" : "import skipped") << ")";
  std::cout << out.str() << std::endl;
  proc.detach();
  return kExitOk;
}

#ifdef _WIN32
int run_start_all(const CliOptions &opts) {
  std::vector<PoolDef> defs;
  std::string error;
  if (!load_pool_defs(default_pool_manifest(), defs, error)) {
    std::cerr << "错误: " << error << std::endl;
    return kExitError;
  }
  char self_buf[32768] = {0};
  const DWORD self_len = GetModuleFileNameA(nullptr, self_buf, sizeof(self_buf));
  if (self_len == 0 || self_len >= sizeof(self_buf)) {
    std::cerr << "错误: 无法获取 gda_pool_ctl 自身路径" << std::endl;
    return kExitError;
  }
  const std::string self_path(self_buf, self_len);
  char home_old[1024] = {0};
  const DWORD home_old_len =
      GetEnvironmentVariableA(kHomeEnvKey, home_old, sizeof(home_old));
  const bool had_home = home_old_len > 0 && home_old_len < sizeof(home_old);
  const std::string home_old_value = had_home ? std::string(home_old) : "";

  std::vector<HANDLE> processes;
  std::vector<std::string> process_names;
  std::vector<std::string> failed;
  for (const PoolDef &def : defs) {
    const fs::path home_dir = fs::path(opts.state_dir) / "home" / def.name;
    std::error_code ec;
    fs::create_directories(home_dir, ec);
    if (ec && !fs::is_directory(home_dir)) {
      failed.push_back(def.name);
      std::cerr << "错误: [" << def.name << "] 无法创建池 home 目录: "
                << home_dir.string() << std::endl;
      continue;
    }
    SetEnvironmentVariableA(kHomeEnvKey, home_dir.string().c_str());
    std::ostringstream cmdline;
    cmdline << "\"" << self_path << "\" start --pool " << def.name
            << " --state-dir \"" << opts.state_dir << "\"";
    if (opts.restart) {
      cmdline << " --restart";
    }
    const std::string text = cmdline.str();
    std::vector<char> cmd(text.begin(), text.end());
    cmd.push_back('\0');
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE, 0,
                        nullptr, nullptr, &si, &pi)) {
      failed.push_back(def.name);
      std::cerr << "错误: [" << def.name << "] 子进程创建失败（错误码 "
                << GetLastError() << "）" << std::endl;
      continue;
    }
    processes.push_back(pi.hProcess);
    process_names.push_back(def.name);
    CloseHandle(pi.hThread);
  }
  SetEnvironmentVariableA(kHomeEnvKey,
                          had_home ? home_old_value.c_str() : nullptr);

  if (!processes.empty()) {
    WaitForMultipleObjects(static_cast<DWORD>(processes.size()),
                           processes.data(), TRUE, INFINITE);
  }
  for (size_t i = 0; i < processes.size(); ++i) {
    DWORD code = 0;
    GetExitCodeProcess(processes[i], &code);
    if (code != 0) {
      failed.push_back(process_names[i]);
    }
    CloseHandle(processes[i]);
  }
  if (!failed.empty()) {
    std::sort(failed.begin(), failed.end());
    failed.erase(std::unique(failed.begin(), failed.end()), failed.end());
    std::ostringstream out;
    out << "错误: " << failed.size() << " 个池启动失败:";
    for (const std::string &pool_name : failed) {
      out << " " << pool_name;
    }
    std::cerr << out.str() << std::endl;
    return kExitFailed;
  }
  return kExitOk;
}
#else
int run_start_all(const CliOptions &) {
  std::cerr << "错误: --all 并发启动仅支持 Windows" << std::endl;
  return kExitError;
}
#endif

int run_main(int argc, char **argv) {
  CliOptions opts;
  std::string error;
  if (parse_args(argc, argv, opts, error) == ParseResult::kError) {
    std::cerr << "错误: " << error << std::endl;
    print_usage(std::cerr);
    return kExitError;
  }
  const bool needs_target =
      opts.command == "start" || opts.command == "stop" ||
      opts.command == "restart";
  if (needs_target && !opts.has_pool && !opts.all) {
    std::cerr << "错误: " << opts.command << " 需要 --pool <名> 或 --all"
              << std::endl;
    print_usage(std::cerr);
    return kExitError;
  }
  if (opts.command == "stop") {
    if (opts.all) {
      return run_stop_all(opts);
    }
    return run_stop_one(opts.pool, opts);
  }
  if (opts.command == "start" || opts.command == "restart") {
    CliOptions start_opts = opts;
    if (opts.command == "restart") {
      start_opts.restart = true;
    }
    if (start_opts.all) {
      return run_start_all(start_opts);
    }
    std::vector<PoolDef> defs;
    if (!load_pool_defs(default_pool_manifest(), defs, error)) {
      std::cerr << "错误: " << error << std::endl;
      return kExitError;
    }
    const PoolDef *def = find_pool_def(defs, start_opts.pool);
    if (def == nullptr) {
      std::cerr << "错误: 池定义中不存在池: " << start_opts.pool << "（"
                << default_pool_manifest() << "）" << std::endl;
      return kExitError;
    }
    return run_start_one(start_opts.pool, *def, start_opts);
  }
  return run_status(opts);
}

} // namespace

int main(int argc, char **argv) {
  try {
    return run_main(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << "错误: 未捕获异常: " << e.what() << std::endl;
    return kExitError;
  }
}
