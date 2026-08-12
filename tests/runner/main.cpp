// gda_test_runner — 自驱动用例跑批器（CLI 入口）
//
// 用法:
//   gda_test_runner [--config-dir <dir>] [--file <name>] [--headless|--gui]
//                   [--no-auto] [--keep-open] [--report-dir <dir>] [--help]
//
// 参数:
//   --config-dir <dir>   JSON 用例目录（默认 PROJECT_ROOT/tests/config）
//   --file <name>        只跑指定用例文件，name 可含或不含 .json 后缀
//                        （默认跑目录下全部 *.json，按文件名排序）
//   --headless           Godot 以 headless 模式启动（默认）
//   --gui                Godot 以窗口模式启动
//                        用例 JSON 的 headless 字段优先于 CLI；冲突时以用例
//                        为准，并在 stderr 提示
//   --no-auto            不启动 Godot 进程；端口取自环境变量
//                        GODOT_AUTOPILOT_PORT，TCP+initialize 就绪后
//                        直连外部 MCP 服务跑用例
//   --keep-open          全部文件跑完后不停止 Godot 进程（进程保留）
//   --report-dir <dir>   报告目录（默认 PROJECT_ROOT/tests/output，自动创建）
//   --help               打印本说明并退出（退出码 0）
//
// 退出码语义:
//   0  全部用例通过
//   1  存在失败/ERROR 的用例文件
//   2  参数或环境错误（未知参数、--headless/--gui 互斥、config-dir 不存在、
//      GODOT_PATH 未配置、--no-auto 端口未设置或未就绪、--file 无匹配、
//      目录无用例、未捕获异常）

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "config_loader.hpp"
#include "godot_process.hpp"
#include "pipeline_executor.hpp"
#include "runner_report.hpp"
#include "../integration/mcp_test_client.hpp"

#ifndef PROJECT_ROOT
#error                                                                         \
    "PROJECT_ROOT 编译宏未定义（参见 tests/CMakeLists.txt 中 gda_engine_tests 的配置）"
#endif

namespace gda_test {

namespace {

namespace fs = std::filesystem;

constexpr int kExitOk = 0;
constexpr int kExitFail = 1;
constexpr int kExitError = 2;

const std::string kDefaultConfigDir = std::string(PROJECT_ROOT) + "/tests/config";
const std::string kDefaultReportDir = std::string(PROJECT_ROOT) + "/tests/output";
const std::string kProjectPath = std::string(PROJECT_ROOT) + "/Example";
const char *kPortEnvKey = "GODOT_AUTOPILOT_PORT";

constexpr auto kPostStopSleep = std::chrono::seconds(2);
constexpr auto kExternalProbeTimeout = std::chrono::seconds(5);

struct CliOptions {
  std::string config_dir = kDefaultConfigDir;
  std::string file_filter;
  bool headless = true;
  bool headless_explicit = false;
  bool no_auto = false;
  bool keep_open = false;
  std::string report_dir = kDefaultReportDir;
};

enum class ParseResult { kOk, kHelp, kError };

void print_usage(std::ostream &out) {
  out << "用法:\n"
      << "  gda_test_runner [--config-dir <dir>] [--file <name>]"
         " [--headless|--gui]\n"
      << "                  [--no-auto] [--keep-open] [--report-dir <dir>]"
         " [--help]\n\n"
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
      << "                      后直连外部 MCP 服务跑用例\n"
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
               arg == "--file") {
      if (i + 1 >= argc) {
        error = "参数 " + arg + " 缺少值";
        return ParseResult::kError;
      }
      const std::string value = argv[++i];
      if (arg == "--config-dir")
        opts.config_dir = value;
      else if (arg == "--report-dir")
        opts.report_dir = value;
      else
        opts.file_filter = value;
    } else {
      error = "未知参数: " + arg;
      return ParseResult::kError;
    }
  }
  return ParseResult::kOk;
}

bool matches_filter(const std::string &filter, const fs::path &file) {
  return filter == file.filename().string() || filter == file.stem().string();
}

// 返回空集合时经 error 说明原因
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

// 返回 -1 表示端口无效/未设置
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

// 跑单个用例文件；stop_after 为 false 时保留 Godot 进程（--keep-open 且为
// 最后一个文件）
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

  GodotProcess proc(GodotProcess::Options{godot_path, kProjectPath, 0, tc.headless});
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
  if (opts.no_auto) {
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
    results.push_back(
        run_one_file(files[i], opts, godot_path, external_port, stop_after));
  }
  if (opts.keep_open) {
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
