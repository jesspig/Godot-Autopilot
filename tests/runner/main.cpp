#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "config_loader.hpp"
#include "godot_process.hpp"
#include "pipeline_executor.hpp"
#include "runner_report.hpp"
#include "../integration/mcp_test_client.hpp"

#include <mcp/JsonValue.hpp>

#ifndef PROJECT_ROOT
#error                                                                         \
    "PROJECT_ROOT 编译宏未定义（参见 tests/CMakeLists.txt 中 gda_test_runner 的配置）"
#endif

namespace gda_test {

namespace {

namespace fs = std::filesystem;

constexpr int kExitOk = 0;
constexpr int kExitFail = 1;
constexpr int kExitError = 2;

const std::string kDefaultConfigDir = std::string(PROJECT_ROOT) + "/tests/config";
const std::string kDefaultReportDir = std::string(PROJECT_ROOT) + "/tests/output";
const std::string kProjectPath = std::string(PROJECT_ROOT) + "/tests/testbed";
const std::string kDefaultPoolStateDir =
    std::string(PROJECT_ROOT) + "/build/debug/tests/pool/state";
const char *kPortEnvKey = "GODOT_AUTOPILOT_PORT";
const char *kPoolCtlFileName = "gda_pool_ctl.exe";

constexpr auto kPostStopSleep = std::chrono::seconds(2);
constexpr auto kExternalProbeTimeout = std::chrono::seconds(5);
constexpr auto kPoolConnectTimeout = std::chrono::seconds(5);

struct CliOptions {
  std::string config_dir = kDefaultConfigDir;
  std::string file_filter;
  bool headless = true;
  bool headless_explicit = false;
  bool no_auto = false;
  bool keep_open = false;
  std::string report_dir = kDefaultReportDir;
  std::string pool;
  bool has_pool = false;
  std::string pool_state_dir = kDefaultPoolStateDir;
};

enum class ParseResult { kOk, kHelp, kError };

bool pool_name_is_valid(const std::string &name) {
  if (name.empty() || name == "." || name == "..")
    return false;
  return name.find('/') == std::string::npos &&
         name.find('\\') == std::string::npos &&
         name.find(':') == std::string::npos;
}

void print_usage(std::ostream &out) {
  out << "用法:\n"
      << "  gda_test_runner [--config-dir <dir>] [--file <name>]"
         " [--headless|--gui]\n"
      << "                  [--no-auto | --pool <名> [--pool-state-dir <目录>]]\n"
      << "                  [--keep-open] [--report-dir <dir>] [--help]\n\n"
      << "参数:\n"
      << "  --config-dir <dir>  JSON 用例目录（默认 " << kDefaultConfigDir
      << "）\n"
      << "  --file <name>       只跑指定用例文件，name 可含或不含 .json 后缀\n"
      << "                      （默认跑目录下全部 *.json，按文件名排序）\n"
      << "  --headless          Godot 以 headless 模式启动（默认）\n"
      << "  --gui               Godot 以窗口模式启动\n"
      << "                      用例 JSON 的 headless 字段优先于 CLI；冲突时\n"
      << "                      以用例为准，并在 stderr 提示\n"
      << "  --no-auto           不启动 Godot 进程；端口取自环境变量\n"
      << "                      GODOT_AUTOPILOT_PORT，TCP+initialize 就绪\n"
      << "                      后直连外部 MCP 服务跑用例；与 --pool 互斥\n"
      << "  --pool <名>         池模式：不启动 Godot 进程，读取池状态文件直连\n"
      << "                      已在运行的池 MCP 服务；连接失败时调用同目录\n"
      << "                      gda_pool_ctl.exe restart 自愈一次。需用 --file\n"
      << "                      指定用例（通常单份），与 --no-auto 互斥\n"
      << "  --pool-state-dir <目录>\n"
      << "                      池状态文件目录（默认 " << kDefaultPoolStateDir
      << "）\n"
      << "                      池模式把 begin/end 用例标记追加到\n"
      << "                      <目录>/home/<池名>/pool_case_log.txt\n"
      << "  --keep-open         全部文件跑完后不停止 Godot 进程（进程保留）\n"
      << "  --report-dir <dir>  报告目录（默认 " << kDefaultReportDir
      << "，自动创建）\n"
      << "  --help              打印本说明并退出\n\n"
      << "退出码:\n"
      << "  0  全部用例通过\n"
      << "  1  存在失败/ERROR 的用例\n"
      << "  2  参数或环境错误\n";
}

ParseResult parse_args(int argc, char **argv, CliOptions &opts,
                       std::string &error) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help")
      return ParseResult::kHelp;
    if (arg == "--headless" || arg == "--gui") {
      if (opts.headless_explicit) {
        error = "--headless 与 --gui 互斥";
        return ParseResult::kError;
      }
      opts.headless = (arg == "--headless");
      opts.headless_explicit = true;
    } else if (arg == "--no-auto") {
      opts.no_auto = true;
    } else if (arg == "--keep-open") {
      opts.keep_open = true;
    } else if (arg == "--config-dir" || arg == "--report-dir" ||
               arg == "--file" || arg == "--pool" ||
               arg == "--pool-state-dir") {
      if (i + 1 >= argc) {
        error = "参数 " + arg + " 缺少值";
        return ParseResult::kError;
      }
      const std::string value = argv[++i];
      if (arg == "--config-dir")
        opts.config_dir = value;
      else if (arg == "--report-dir")
        opts.report_dir = value;
      else if (arg == "--pool") {
        opts.pool = value;
        opts.has_pool = true;
      } else if (arg == "--pool-state-dir")
        opts.pool_state_dir = value;
      else
        opts.file_filter = value;
    } else {
      error = "未知参数: " + arg;
      return ParseResult::kError;
    }
  }
  if (opts.has_pool && opts.no_auto) {
    error = "--pool 与 --no-auto 互斥";
    return ParseResult::kError;
  }
  if (opts.has_pool && !pool_name_is_valid(opts.pool)) {
    error = "--pool 含非法路径字符: " + opts.pool;
    return ParseResult::kError;
  }
  if (opts.has_pool && opts.file_filter.empty()) {
    error = "池模式需要 --file 指定用例";
    return ParseResult::kError;
  }
  return ParseResult::kOk;
}

bool matches_filter(const std::string &filter, const fs::path &file) {
  return filter == file.filename().string() || filter == file.stem().string();
}

std::vector<fs::path> collect_test_files(const std::string &config_dir,
                                         const std::string &filter,
                                         std::string &error) {
  std::vector<fs::path> files;
  for (const auto &entry : fs::directory_iterator(config_dir)) {
    if (!entry.is_regular_file())
      continue;
    const fs::path &path = entry.path();
    if (path.extension() != ".json")
      continue;
    if (filter.empty() || matches_filter(filter, path))
      files.push_back(path);
  }
  std::sort(files.begin(), files.end());
  if (files.empty()) {
    error = filter.empty()
                ? "目录中未找到任何 *.json 用例: " + config_dir
                : "未找到匹配 --file 的用例: " + filter;
  }
  return files;
}

struct PoolState {
  long long pid = 0;
  int port = 0;
};

bool read_pool_state(const fs::path &file, PoolState &state,
                     std::string &error) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    error = "无法打开池状态文件: " + file.string();
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  mcp::JsonValue root;
  try {
    root = mcp::JsonValue::Parse(buffer.str());
  } catch (const std::exception &) {
    error = "池状态文件不是合法 JSON: " + file.string();
    return false;
  }
  if (!root.IsObject()) {
    error = "池状态文件顶层必须是 object: " + file.string();
    return false;
  }
  const mcp::JsonValue *pid = root.Find("pid");
  const mcp::JsonValue *port = root.Find("port");
  if (!pid || !pid->IsInt() || pid->GetInt() <= 0) {
    error = "池状态文件 pid 字段缺失或非法: " + file.string();
    return false;
  }
  if (!port || !port->IsInt() || port->GetInt() <= 0 ||
      port->GetInt() > 65535) {
    error = "池状态文件 port 字段缺失或非法: " + file.string();
    return false;
  }
  state.pid = pid->GetInt();
  state.port = static_cast<int>(port->GetInt());
  return true;
}

long long epoch_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string case_log_line(const std::string &body) {
  return "[" + std::to_string(epoch_ms()) + "] " + body;
}

void append_pool_case_log(const fs::path &log_file, const std::string &line) {
  std::error_code ec;
  fs::create_directories(log_file.parent_path(), ec);
  std::ofstream out(log_file, std::ios::binary | std::ios::app);
  if (!out) {
    std::cerr << "警告: 无法写入池用例标记: " << log_file.string() << std::endl;
    return;
  }
  out << line << "\n";
}

#ifdef _WIN32
bool restart_pool(const std::string &pool, const std::string &state_dir,
                  std::string &error) {
  char self_buf[32768] = {0};
  const DWORD self_len = GetModuleFileNameA(nullptr, self_buf, sizeof(self_buf));
  if (self_len == 0 || self_len >= static_cast<DWORD>(sizeof(self_buf))) {
    error = "无法获取 gda_test_runner 自身路径";
    return false;
  }
  fs::path ctl_path(self_buf);
  ctl_path.replace_filename(kPoolCtlFileName);
  std::ostringstream cmdline;
  cmdline << "\"" << ctl_path.string() << "\" restart --pool " << pool
          << " --state-dir \"" << state_dir << "\"";
  const std::string text = cmdline.str();
  std::vector<char> cmd(text.begin(), text.end());
  cmd.push_back('\0');
  STARTUPINFOA si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      nullptr, &si, &pi)) {
    error = "gda_pool_ctl 启动失败（错误码 " + std::to_string(GetLastError()) +
            "）";
    return false;
  }
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 0;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  if (code != 0) {
    error = "gda_pool_ctl restart 退出码 " + std::to_string(code);
    return false;
  }
  return true;
}
#else
bool restart_pool(const std::string &, const std::string &, std::string &error) {
  error = "池自愈重启仅支持 Windows";
  return false;
}
#endif

int resolve_external_port() {
  const char *raw = std::getenv(kPortEnvKey);
  if (!raw || !*raw)
    return -1;
  try {
    const int port = std::stoi(raw);
    if (port <= 0 || port > 65535)
      return -1;
    return port;
  } catch (const std::exception &) {
    return -1;
  }
}

FileResult make_error_result(const std::string &name, long long elapsed_ms,
                              const std::string &message) {
  FileResult result;
  result.name = name;
  result.duration_ms = static_cast<double>(elapsed_ms);
  result.fatal_error = message;
  return result;
}

FileResult run_one_file(const fs::path &file, const CliOptions &opts,
                        const std::string &godot_path, int external_port,
                        bool stop_after) {
  const std::string name = file.stem().string();
  const auto start = std::chrono::steady_clock::now();
  const auto elapsed_ms = [&start] {
    return static_cast<long long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start)
            .count());
  };
  std::cout << "[" << name << "] starting..." << std::endl;

  TestCase tc;
  try {
    tc = load_test_case(file.string());
  } catch (const std::exception &e) {
    FileResult result =
        make_error_result(name, elapsed_ms(), "用例加载失败: " + std::string(e.what()));
    std::cout << "[" << name << "] FAIL (0 steps, " << result.duration_ms
              << " ms)" << std::endl;
    return result;
  }

  if (opts.headless_explicit && tc.headless != opts.headless) {
    std::cerr << "[" << name << "] 警告: 用例 headless 字段与 CLI --"
              << (opts.headless ? "headless" : "gui")
              << " 冲突，以用例为准（headless="
              << (tc.headless ? "true" : "false") << "）" << std::endl;
  }

  if (opts.no_auto) {
    if (!tc.requires.empty()) {
      std::cerr << "[" << name << "] 警告: --no-auto 模式下 requires 不生效，"
                   "授权由外部 MCP 服务决定" << std::endl;
    }
    McpTestClient client(external_port);
    if (!client.connect(kExternalProbeTimeout)) {
      FileResult result = make_error_result(
          name, elapsed_ms(),
          "外部 MCP 服务未就绪（端口 " + std::to_string(external_port) + "）");
      std::cout << "[" << name << "] FAIL (0 steps, " << result.duration_ms
                << " ms)" << std::endl;
      return result;
    }
    FileResult result = run_pipeline(tc, nullptr, &client);
    std::cout << "[" << name << "] " << (result.passed ? "PASS" : "FAIL")
              << " (" << result.steps.size() << " steps, " << result.duration_ms
              << " ms)" << std::endl;
    return result;
  }

  std::string allow_list;
  for (size_t i = 0; i < tc.requires.size(); ++i) {
    if (i > 0) allow_list += ',';
    allow_list += tc.requires[i];
  }

  GodotProcess proc(GodotProcess::Options{godot_path, kProjectPath, 0,
                                          tc.headless, allow_list});
  if (!proc.start()) {
    FileResult result = make_error_result(
        name, elapsed_ms(), "Godot 启动失败: " + proc.last_error());
    std::cout << "[" << name << "] FAIL (0 steps, " << result.duration_ms
              << " ms)" << std::endl;
    return result;
  }
  FileResult result = run_pipeline(tc, &proc);
  if (stop_after) {
    proc.stop();
    std::this_thread::sleep_for(kPostStopSleep);
  }
  std::cout << "[" << name << "] " << (result.passed ? "PASS" : "FAIL")
            << " (" << result.steps.size() << " steps, " << result.duration_ms
            << " ms)" << std::endl;
  return result;
}

struct PoolRunOutcome {
  FileResult result;
  bool fatal = false;
};

PoolRunOutcome run_one_file_pool(const fs::path &file, const CliOptions &opts,
                                 const fs::path &state_file, int &pool_port,
                                 const fs::path &case_log) {
  const std::string name = file.stem().string();
  const auto start = std::chrono::steady_clock::now();
  const auto elapsed_ms = [&start] {
    return static_cast<long long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start)
            .count());
  };
  PoolRunOutcome outcome;
  append_pool_case_log(case_log, case_log_line("begin " + name));
  std::cout << "[" << name << "] starting..." << std::endl;

  const auto fail = [&](const std::string &message) {
    std::cerr << "错误: [" << name << "] " << message << std::endl;
    outcome.result = make_error_result(name, elapsed_ms(), message);
    outcome.fatal = true;
    std::cout << "[" << name << "] FAIL (0 steps, " << outcome.result.duration_ms
              << " ms)" << std::endl;
    append_pool_case_log(case_log, case_log_line("end " + name + " passed=0"));
    return outcome;
  };

  TestCase tc;
  try {
    tc = load_test_case(file.string());
  } catch (const std::exception &e) {
    outcome.result = make_error_result(
        name, elapsed_ms(), "用例加载失败: " + std::string(e.what()));
    std::cout << "[" << name << "] FAIL (0 steps, " << outcome.result.duration_ms
              << " ms)" << std::endl;
    append_pool_case_log(case_log, case_log_line("end " + name + " passed=0"));
    return outcome;
  }

  if (opts.headless_explicit && tc.headless != opts.headless) {
    std::cerr << "[" << name << "] 警告: 用例 headless 字段与 CLI --"
              << (opts.headless ? "headless" : "gui")
              << " 冲突，以用例为准（headless="
              << (tc.headless ? "true" : "false") << "）" << std::endl;
  }
  if (!tc.requires.empty()) {
    std::cerr << "[" << name << "] 池模式授权由池决定，忽略 requires 声明"
              << std::endl;
  }

  McpTestClient client(pool_port);
  std::unique_ptr<McpTestClient> healed;
  if (!client.connect(kPoolConnectTimeout)) {
    std::cerr << "[" << name << "] 池 MCP 连接失败（端口 " << pool_port
              << "），调用 gda_pool_ctl restart 自愈" << std::endl;
    std::string restart_error;
    if (!restart_pool(opts.pool, opts.pool_state_dir, restart_error)) {
      return fail("池自愈失败: " + restart_error);
    }
    int retry_port = pool_port;
    PoolState restarted;
    std::string state_error;
    if (read_pool_state(state_file, restarted, state_error)) {
      retry_port = restarted.port;
      pool_port = retry_port;
    }
    healed = std::make_unique<McpTestClient>(retry_port);
    if (!healed->connect(kPoolConnectTimeout)) {
      return fail("池自愈后 MCP 服务仍未就绪（端口 " +
                  std::to_string(retry_port) + "）");
    }
  }
  McpTestClient *active = healed ? healed.get() : &client;
  outcome.result = run_pipeline(tc, nullptr, active);
  std::cout << "[" << name << "] " << (outcome.result.passed ? "PASS" : "FAIL")
            << " (" << outcome.result.steps.size() << " steps, "
            << outcome.result.duration_ms << " ms)" << std::endl;
  append_pool_case_log(
      case_log,
      case_log_line("end " + name +
                    (outcome.result.passed ? " passed=1" : " passed=0")));
  return outcome;
}

int run_main(int argc, char **argv) {
  CliOptions opts;
  std::string error;
  const ParseResult parsed = parse_args(argc, argv, opts, error);
  if (parsed == ParseResult::kHelp) {
    print_usage(std::cout);
    return kExitOk;
  }
  if (parsed == ParseResult::kError) {
    std::cerr << "错误: " << error << std::endl;
    print_usage(std::cerr);
    return kExitError;
  }

  if (!fs::is_directory(opts.config_dir)) {
    std::cerr << "错误: config-dir 不存在: " << opts.config_dir << std::endl;
    return kExitError;
  }

  std::string godot_path;
  int external_port = 0;
  int pool_port = 0;
  fs::path pool_state_file;
  fs::path pool_case_log;
  if (opts.has_pool) {
    pool_state_file = fs::path(opts.pool_state_dir) / (opts.pool + ".json");
    std::error_code state_ec;
    if (!fs::is_regular_file(pool_state_file, state_ec)) {
      std::cerr << "错误: 池状态文件不存在: " << pool_state_file.string()
                << "，请先运行 gda_pool_ctl start --pool " << opts.pool
                << std::endl;
      return kExitError;
    }
    PoolState state;
    std::string state_error;
    if (!read_pool_state(pool_state_file, state, state_error)) {
      std::cerr << "错误: " << state_error << std::endl;
      return kExitError;
    }
    pool_port = state.port;
    pool_case_log =
        fs::path(opts.pool_state_dir) / "home" / opts.pool / "pool_case_log.txt";
  } else if (opts.no_auto) {
    external_port = resolve_external_port();
    if (external_port <= 0) {
      std::cerr << "错误: --no-auto 需要环境变量 " << kPortEnvKey
                << " 指定外部 MCP 端口" << std::endl;
      return kExitError;
    }
    McpTestClient probe(external_port);
    if (!probe.connect(kExternalProbeTimeout)) {
      std::cerr << "错误: 外部 MCP 服务未就绪（端口 " << external_port << "）"
                << std::endl;
      return kExitError;
    }
  } else {
    godot_path = GodotProcess::resolve_godot_path();
    if (godot_path.empty()) {
      std::cerr << "错误: 未找到 Godot 可执行文件。请设置 GODOT_PATH 环境"
                   "变量，或参照 .env.template 在仓库根 .env 中配置 "
                   "GODOT_PATH"
                << std::endl;
      return kExitError;
    }
    const fs::path bed = fs::path(kProjectPath);
    if (!fs::is_regular_file(bed / "project.godot") ||
        !fs::is_regular_file(bed / "addons" / "godot-autopilot" /
                                     "godot-autopilot.gdextension")) {
      std::cerr << "错误: L2 测试床未就绪: " << kProjectPath
                << " 缺少 project.godot 或 addons/godot-autopilot/"
                   "godot-autopilot.gdextension，请先运行 uv run main.py build "
                   "与 uv run main.py deploy"
                << std::endl;
      return kExitError;
    }
  }

  const std::vector<fs::path> files =
      collect_test_files(opts.config_dir, opts.file_filter, error);
  if (files.empty()) {
    std::cerr << "错误: " << error << std::endl;
    return kExitError;
  }

  std::vector<FileResult> results;
  results.reserve(files.size());
  for (size_t i = 0; i < files.size(); ++i) {
    const bool stop_after = !opts.keep_open || i + 1 < files.size();
    if (opts.has_pool) {
      const PoolRunOutcome outcome = run_one_file_pool(
          files[i], opts, pool_state_file, pool_port, pool_case_log);
      if (outcome.fatal) {
        return kExitError;
      }
      results.push_back(outcome.result);
    } else {
      results.push_back(
          run_one_file(files[i], opts, godot_path, external_port, stop_after));
    }
  }
  if (opts.keep_open && !opts.has_pool) {
    std::cout << "进程保留（--keep-open），Godot 进程未停止" << std::endl;
  }

  print_console_report(results);
  const std::string report_path = save_json_report(results, opts.report_dir);
  std::cout << "JSON 报告: " << report_path << std::endl;

  const bool all_passed =
      std::all_of(results.begin(), results.end(),
                  [](const FileResult &r) { return r.passed; });
  return all_passed ? kExitOk : kExitFail;
}

} // namespace

} // namespace gda_test

int main(int argc, char **argv) {
  try {
    return gda_test::run_main(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << "错误: 未捕获异常: " << e.what() << std::endl;
    return 2;
  }
}
