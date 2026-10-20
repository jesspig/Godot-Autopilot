import path from "node:path";

import type { GlobalOptions } from "../args";
import { ExitCode } from "../errors";
import {
  deriveEndpoint,
  endpointBaseUrl,
  findPluginRecordByPort,
  findPluginRecordByProject,
  type EndpointEntry,
  type PluginInstanceRecord,
} from "../instances";
import { parseProbeLevel, probeEndpoint } from "../probe";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type ResolvedTarget, type TargetContext } from "../target";
import { writeJson } from "./json-output";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

export async function run(_positional: string[], opts: GlobalOptions): Promise<number> {
  const target = resolveTarget(opts, TARGET_CONTEXT);
  const record = locateRecord(target, opts);
  const mcp = record?.mcpEndpoint ?? deriveEndpoint(target.port, "mcp");
  const api = record?.apiEndpoint ?? deriveEndpoint(target.port, "api");
  const level = parseProbeLevel(opts.probe);
  const alive = await probeEndpoint(
    { host: api.host, port: api.port, url: endpointBaseUrl(api) },
    level,
  );
  writeJson(
    {
      port: target.port,
      source: target.source,
      chain: target.chain,
      mcp: endpointView(mcp),
      api: endpointView(api),
      alive,
      probe: level,
    },
    opts,
  );
  return ExitCode.Success;
}

function locateRecord(target: ResolvedTarget, opts: GlobalOptions): PluginInstanceRecord | undefined {
  return (
    target.pluginRecord ??
    findPluginRecordByPort(target.port) ??
    findPluginRecordByProject(opts.project !== undefined ? path.resolve(opts.project) : process.cwd())
  );
}

function endpointView(endpoint: EndpointEntry): Record<string, unknown> {
  return {
    url: endpointBaseUrl(endpoint),
    host: endpoint.host,
    port: endpoint.port,
    path: endpoint.path,
    protocol: endpoint.protocol,
    auth: endpoint.auth,
  };
}
