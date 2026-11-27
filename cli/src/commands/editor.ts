import type { GlobalOptions } from "../args";
import { nativeCapabilities, nativeCatalogList, nativeHello } from "../discovery";
import { CliError, ExitCode } from "../errors";
import { findById, findLiveByProject } from "../registry";
import { resolveNativeTarget, type TargetContext } from "../target";
import { writeJson } from "./json-output";
import { run as runList } from "./list";
import { run as runRestart } from "./restart";
import { run as runStart } from "./start";
import { run as runStop } from "./stop";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

function requiresNativeEndpoint(): CliError {
  return new CliError(
    ExitCode.Transport,
    "no native IPC endpoint is available for this instance; pass --socket or --id from 'gda editor list'.",
    "the native control plane is the only supported channel.",
  );
}

function nativeIdentity(target: ReturnType<typeof resolveNativeTarget>): string {
  return target.instanceId ?? target.pluginRecord?.instanceId ?? target.instance?.id ?? "";
}

export async function editorStatus(opts: GlobalOptions): Promise<number> {
  try {
    const target = resolveNativeTarget(opts, TARGET_CONTEXT);
    if (target.ipcEndpoint === undefined) throw requiresNativeEndpoint();
    const identity = nativeIdentity(target);
    const hello = await nativeHello(target.ipcEndpoint, identity);
    const capabilities = await nativeCapabilities(target.ipcEndpoint, hello.instance_id || identity);
    const tools = await nativeCatalogList(target.ipcEndpoint, hello.instance_id || identity);
    writeJson(
      {
        schema_version: 1,
        ok: true,
        instance: { id: hello.instance_id || identity, pid: target.instance?.pid ?? target.pluginRecord?.pid ?? null },
        project: target.pluginRecord?.projectPath ?? target.instance?.project ?? "",
        version: target.pluginRecord?.version ?? target.pluginVersion ?? "",
        ipc_kind: target.ipcEndpoint.kind ?? "",
        ready: true,
        tool_count: tools.length,
        capabilities,
      },
      opts,
    );
    return ExitCode.Success;
  } catch (error) {
    if (error instanceof CliError) {
      writeJson(
        {
          schema_version: 1,
          ok: false,
          error: error.message,
          instance: { id: "", pid: null },
          project: "",
          version: "",
          ipc_kind: "",
          ready: false,
          tool_count: 0,
          capabilities: {},
        },
        opts,
      );
      return error.code;
    }
    throw error;
  }
}

export async function editorPing(opts: GlobalOptions): Promise<number> {
  try {
    const target = resolveNativeTarget(opts, TARGET_CONTEXT);
    if (target.ipcEndpoint === undefined) throw requiresNativeEndpoint();
    const identity = nativeIdentity(target);
    const hello = await nativeHello(target.ipcEndpoint, identity);
    writeJson(
      {
        schema_version: 1,
        ok: true,
        pong: true,
        instance: hello.instance_id || identity,
        ipc_kind: target.ipcEndpoint.kind ?? "",
      },
      opts,
    );
    return ExitCode.Success;
  } catch (error) {
    if (error instanceof CliError) {
      writeJson({ schema_version: 1, ok: false, pong: false, error: error.message }, opts);
      return error.code;
    }
    throw error;
  }
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0];
  if (sub === undefined) {
    throw new CliError(ExitCode.Usage, "editor requires a subcommand: status/list/start/stop/restart/ping.");
  }
  const rest = positional.slice(1);
  switch (sub) {
    case "list":
      return runList(rest, opts);
    case "start":
      return runStart(rest, opts);
    case "stop":
      return runStop(rest, opts);
    case "restart":
      return runRestart(rest, opts);
    case "status":
      return editorStatus(opts);
    case "ping":
      return editorPing(opts);
    default:
      throw new CliError(ExitCode.Usage, `unknown editor subcommand: ${sub} (expected status/list/start/stop/restart/ping)`);
  }
}
