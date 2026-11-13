import { parseTransport, type GlobalOptions } from "../args";
import { CLI_VERSION } from "../version";
import { CliError, ExitCode } from "../errors";
import { callTool, ensureConnection, getHandshakeInfo } from "../mcp/client";
import { assertCallFamilyUsage, callTimeoutSec, extractPayload } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type ResolvedTarget, type TargetContext } from "../target";
import { writeJson } from "./json-output";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

function fnv1a(text: string): string {
  let hash = 0x811c9dc5;
  for (let index = 0; index < text.length; index += 1) {
    hash ^= text.charCodeAt(index);
    hash = Math.imul(hash, 0x01000193);
  }
  return (hash >>> 0).toString(16).padStart(8, "0");
}

function asRecord(value: unknown): Record<string, unknown> {
  if (value !== null && typeof value === "object" && !Array.isArray(value)) {
    return value as Record<string, unknown>;
  }
  return {};
}

function buildStatus(
  target: ResolvedTarget,
  payload: Record<string, unknown>,
  transport: "ipc" | "http",
): Record<string, unknown> {
  const pluginVersion =
    typeof payload.version === "string" ? payload.version : target.pluginVersion;
  const toolCount =
    typeof payload.tool_count === "number" ? payload.tool_count : 0;
  return {
    schema_version: 1,
    ok: true,
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
    mcp: getHandshakeInfo(target.port),
    port: target.port,
    port_source: target.portSource,
    mcp_url: target.mcpUrl,
    transport,
    ipc_kind: target.ipcEndpoint?.kind ?? "",
    toolset_hash: fnv1a(`${pluginVersion}:${toolCount}`),
    server: payload,
  };
}

export async function run(_positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("status", opts);
  const transportMode = parseTransport(opts.transportText);
  try {
    const target = resolveTarget(opts, TARGET_CONTEXT);
    let actualTransport: "ipc" | "http" = target.transport;
    if (transportMode !== "ipc") {
      await ensureConnection(target.port, callTimeoutSec(opts));
    }
    const envelope = await callTool("system_status", {}, {
      port: target.port,
      timeoutSec: callTimeoutSec(opts),
      ipcEndpoint: target.ipcEndpoint,
      transport: transportMode,
      noFallback: opts.noFallback,
      transportSink: (transport) => {
        actualTransport = transport;
      },
    });
    const extracted = extractPayload(envelope, { saveImages: opts.saveImages, raw: opts.raw });
    const payload = asRecord(extracted.display);
    const status = buildStatus(target, payload, actualTransport);
    writeJson(status, opts);
    return extracted.toolFailed ? ExitCode.ToolFailure : ExitCode.Success;
  } catch (error) {
    if (error instanceof CliError) {
      writeJson(
        {
          schema_version: 1,
          ok: false,
          cli_version: CLI_VERSION,
          error: error.message,
          port: null,
          port_source: "",
          mcp_url: null,
          transport: null,
          ipc_kind: "",
          toolset_hash: "",
        },
        opts,
      );
      return error.code;
    }
    throw error;
  }
}
