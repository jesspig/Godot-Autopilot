import { CliError, ExitCode } from "./errors";
import { CLI_VERSION } from "./version";

export interface GlobalOptions {
  portText?: string;
  id?: string;
  project?: string;
  pretty?: boolean;
  raw?: boolean;
  quiet?: boolean;
  timeoutText?: string;
  category?: string;
  tagsText?: string;
  argsJson?: string;
  argsFile?: string;
  saveImages?: string;
  help?: boolean;
  version?: boolean;
  godot?: string;
  waitText?: string;
  tailText?: string;
  headless?: boolean;
  play?: boolean;
  noImport?: boolean;
  force?: boolean;
}

const STRING_FLAGS: Record<string, keyof GlobalOptions> = {
  "--port": "portText",
  "--id": "id",
  "--project": "project",
  "--timeout": "timeoutText",
  "--category": "category",
  "--tags": "tagsText",
  "--args": "argsJson",
  "--args-file": "argsFile",
  "--save-images": "saveImages",
  "--godot": "godot",
  "--wait": "waitText",
  "--tail": "tailText",
};

const BOOLEAN_FLAGS: Record<string, keyof GlobalOptions> = {
  "--pretty": "pretty",
  "--raw": "raw",
  "--quiet": "quiet",
  "--help": "help",
  "--version": "version",
  "--headless": "headless",
  "--play": "play",
  "--no-import": "noImport",
  "--force": "force",
};

export function parseArgv(argv: string[]): { opts: GlobalOptions; positionals: string[] } {
  const opts: Record<string, string | boolean | undefined> = {};
  const positionals: string[] = [];
  let onlyPositionals = false;
  let i = 0;
  while (i < argv.length) {
    const token = argv[i];
    if (onlyPositionals) {
      positionals.push(token);
      i += 1;
    } else if (token === "--") {
      onlyPositionals = true;
      i += 1;
    } else if (token.startsWith("-")) {
      const eq = token.indexOf("=");
      const name = eq === -1 ? token : token.slice(0, eq);
      const stringKey = STRING_FLAGS[name];
      const boolKey = BOOLEAN_FLAGS[name];
      if (stringKey !== undefined) {
        let value: string;
        if (eq !== -1) {
          value = token.slice(eq + 1);
        } else {
          const next = argv[i + 1];
          if (next === undefined || next.startsWith("-")) {
            throw new CliError(ExitCode.Usage, `选项 ${name} 缺少值`);
          }
          value = next;
          i += 1;
        }
        opts[stringKey] = value;
      } else if (boolKey !== undefined) {
        if (eq !== -1) {
          throw new CliError(ExitCode.Usage, `布尔选项 ${name} 不接受值`);
        }
        opts[boolKey] = true;
      } else {
        throw new CliError(ExitCode.Usage, `未知选项: ${name}`);
      }
      i += 1;
    } else {
      positionals.push(token);
      i += 1;
    }
  }
  return { opts: opts as GlobalOptions, positionals };
}

export function usageText(): string {
  return `gda ${CLI_VERSION} — Godot Autopilot CLI

用法:
  gda [全局选项] <命令> [参数...]

命令:
  ping                测试与 MCP 服务的连通性
  status              显示服务状态与工程信息
  categories          列出工具类别
  search <关键词>     搜索工具
  describe <工具名>   显示工具详情
  call <工具名>       调用工具
  list                列出已启动的 Godot 进程
  start               启动 Godot 进程
  stop                停止 Godot 进程
  logs                查看进程日志

全局选项:
  --port <text>       MCP 服务端口（默认 9527）
  --id <text>         目标服务实例 ID
  --project <text>    工程路径
  --pretty            美化 JSON 输出（缩进 2 空格）
  --raw              原始 JSON 输出
  --quiet            静默模式
  --timeout <text>    调用超时（秒）
  --category <text>   按类别过滤
  --tags <text>       按标签过滤（逗号分隔）
  --args <json>       工具调用参数（JSON 字符串）
  --args-file <path>  工具调用参数（JSON 文件）
  --save-images <dir> 保存返回的图片到本地目录
  --godot <path>      显式指定 Godot 可执行文件（start）
  --headless          以无头模式启动编辑器（start）
  --play              就绪后播放当前场景（start）
  --no-import          跳过启动时的资源导入（start）
  --wait <sec>        就绪/停止等待秒数，0=不等待（start/stop）
  --force             强制停止进程（stop）
  --tail <n>          日志末尾行数（logs）
  --help              显示本帮助
  --version           显示版本

退出码:
  0  成功
  1  工具失败
  2  传输失败
  3  用法错误`;
}
