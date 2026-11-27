import type { GlobalOptions } from "../args";
import { fnv1a32 } from "../hash";
import { CLI_VERSION } from "../version";
import { CliError, ExitCode } from "../errors";
import { callTool } from "../rpc/client";
import { assertCallFamilyUsage, callTimeoutSec, extractPayload } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type ResolvedTarget, type TargetContext } from "../target";
import { writeJson } from "./json-output";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

function asRecord(value: unknown): Record<string, unknown> {
  if (value !== null && typeof value === "object" && !Array.isArray(value)) {
    return value as Record<string, unknown>;
  }
  return {};
}

function buildStatus(target: ResolvedTarget, payload: Record<string, unknown>): Record<string, unknown> {
  const pluginVersion =
    typeof payload.version === "string" ? payload.version : target.pluginVersion;
  const toolCount =
    typeof payload.tool_count === "number" ? payload.tool_count : 0;
  return {
    schema_version: 1,
    ok: true,
    command: "editor.status",
    project:
      target.pluginRecord?.projectPath ??
      target.instance?.project ??
      (payload.project as string | undefined) ??
      "",
    pid: target.pluginRecord?.pid ?? target.instance?.pid ?? null,
    instance_id: target.pluginRecord?.instanceId ?? target.instance?.id ?? "",
    plugin_version: pluginVersion,
    cli_version: CLI_VERSION,
    protocol_version: "1",
    port: target.port,
    port_source: target.portSource,
    ipc_kind: target.ipcEndpoint?.kind ?? "",
    toolset_hash: fnv1a32(`${pluginVersion}:${toolCount}`),
    server: payload,
  };
}

export async function run(_positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("status", opts);
  try {
    const target = resolveTarget(opts, TARGET_CONTEXT);
    const envelope = await callTool("system_status", {}, {
      port: target.port,
      timeoutSec: callTimeoutSec(opts),
      ipcEndpoint: target.ipcEndpoint,
      traceparent: undefined,
    });
    const extracted = extractPayload(envelope, { saveImages: opts.saveImages, raw: opts.raw });
    const payload = asRecord(extracted.display);
    const status = buildStatus(target, payload);
    writeJson(status, opts, "editor.status", !extracted.toolFailed);
    return extracted.toolFailed ? ExitCode.ToolFailure : ExitCode.Success;
  } catch (error) {
    if (error instanceof CliError) {
      writeJson(
        {
          schema_version: 1,
          ok: false,
          command: "editor.status",
          cli_version: CLI_VERSION,
          error: error.message,
          port: null,
          port_source: "",
          ipc_kind: "",
          toolset_hash: "",
        },
        opts,
        "editor.status",
        false,
      );
      return error.code;
    }
    throw error;
  }
}
