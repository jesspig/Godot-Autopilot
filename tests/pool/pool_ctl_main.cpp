#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "../integration/mcp_test_client.hpp"

#include <mcp/JsonValue.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef PROJECT_ROOT
#error                                                                         \
    "PROJECT_ROOT 编译宏未定义（tests/CMakeLists.txt 已为 gda_pool_ctl 配置）"
#endif

namespace {

namespace fs = std::filesystem;

constexpr int kExitOk = 0;
constexpr int kExitError = 2;
constexpr auto kStatusProbeTimeout = std::chrono::seconds(5);
constexpr char kNoAlivePoolText[] = "no alive pool";

std::string default_state_dir() {
  return std::string(PROJECT_ROOT) + "/build/debug/tests/pool/state";
}

struct CliOptions {
  std::string command;
  std::string pool;
  bool has_pool = false;
  bool all = false;
  std::string state_dir = default_state_dir();
};

enum class ParseResult { kOk, kError };

void print_usage(std::ostream &out) {
  out << "用法:\n"
      << "  gda_pool_ctl <start|stop|status|restart> [--pool <名>|--all]"
         " [--state-dir <目录>]\n\n"
      << "子命令:\n"
      << "  start               启动池编辑器会话（T-04 实现）\n"
      << "  stop                停止池并清理状态文件（T-05 实现）\n"
      << "  status              枚举状态文件并报告各池存活状态\n"
      << "  restart             重启池（T-05 实现）\n\n"
      << "参数:\n"
      << "  --pool <名>         只操作指定池（状态文件名去 .json 后缀）\n"
      << "  --all               操作全部池，与 --pool 互斥\n"
      << "  --state-dir <目录>  池状态文件目录（默认 " << default_state_dir()
      << "）\n\n"
      << "退出码:\n"
      << "  0  成功\n"
      << "  2  参数或环境错误\n";
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

int run_main(int argc, char **argv) {
  CliOptions opts;
  std::string error;
  if (parse_args(argc, argv, opts, error) == ParseResult::kError) {
    std::cerr << "错误: " << error << std::endl;
    print_usage(std::cerr);
    return kExitError;
  }
  if (opts.command == "start" || opts.command == "stop" ||
      opts.command == "restart") {
    std::cerr << opts.command << " not implemented (T-04/T-05)" << std::endl;
    return kExitError;
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
