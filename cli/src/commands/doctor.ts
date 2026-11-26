import type { GlobalOptions } from "../args";
import {
  hasOfflineCatalog,
  nativeHello,
  readOfflineCatalog,
} from "../discovery";
import { CliError, ExitCode } from "../errors";
import { findById, findLiveByProject, loadRegistry } from "../registry";
import { isAlive } from "../process";
import { resolveNativeTarget, type TargetContext } from "../target";
import { writeJson } from "./json-output";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

export type CheckStatus = "ok" | "warn" | "error";

export interface DoctorCheck {
  id: string;
  code: string;
  status: CheckStatus;
  detail: string;
  remediation: string;
}

export interface DoctorReport {
  schema_version: number;
  ok: boolean;
  mode: "full" | "ci";
  target?: Record<string, unknown>;
  checks: DoctorCheck[];
  summary: { ok: number; warn: number; error: number };
}

function check(id: string, code: string, status: CheckStatus, detail: string, remediation: string): DoctorCheck {
  return { id, code, status, detail, remediation };
}

function ciDetail(text: string): string {
  return text.replace(/[A-Za-z]:[\\/][^\s,;]+/g, "<path>").replace(/\/[\w.-]+\/[\w.-]+/g, "<path>");
}

function finalize(report: DoctorReport): DoctorReport {
  report.summary = {
    ok: report.checks.filter((item) => item.status === "ok").length,
    warn: report.checks.filter((item) => item.status === "warn").length,
    error: report.checks.filter((item) => item.status === "error").length,
  };
  report.ok = report.summary.error === 0;
  if (report.mode === "ci") {
    report.checks = report.checks.map((item) => ({ ...item, detail: ciDetail(item.detail) }));
  }
  return report;
}

function registryCheck(): DoctorCheck {
  try {
    const data = loadRegistry();
    return check("registry", "GDA_REGISTRY_OK", "ok", `${data.instances.length} registered instance(s)`, "none");
  } catch (error) {
    return check(
      "registry",
      "GDA_REGISTRY_UNREADABLE",
      "error",
      (error as Error).message,
      "Repair or remove the CLI instances registry, then retry.",
    );
  }
}

function instanceChecks(instances: Array<{ id: string; pid: number; project: string; ipcEndpoint?: unknown }>): DoctorCheck[] {
  if (instances.length === 0) {
    return [check("instances", "GDA_NO_INSTANCE", "warn", "no registered instance", "Run 'gda editor start --project <dir>' or open the editor with the plugin enabled.")];
  }
  const live = instances.filter((entry) => entry.pid > 0 && isAlive(entry.pid));
  if (live.length === 0) {
    return [check("instances", "GDA_STALE_INSTANCE", "error", `${instances.length} stale instance(s)`, "Run 'gda editor list --stale' and remove stale registrations.")];
  }
  return [check("instances", "GDA_INSTANCE_OK", "ok", `${live.length} live instance(s)`, "none")];
}

function nativeCheck(instances: Array<{ id: string; pid: number; ipcEndpoint?: { kind?: string; path?: string; name?: string; host?: string; port?: number } | null }>): DoctorCheck {
  const candidate = instances.find((entry) => entry.ipcEndpoint != null && isAlive(entry.pid));
  if (candidate === undefined) {
    return check("native", "GDA_IPC_ENDPOINT_MISSING", "warn", "no live instance advertises a native IPC endpoint", "Restart the editor so the plugin binds its native IPC channel; the doctor never uses MCP initialize or HTTP /mcp.");
  }
  return check("native", "GDA_IPC_ENDPOINT_OK", "ok", `${candidate.id} exposes ${candidate.ipcEndpoint?.kind ?? "unknown"}`, "none");
}

function catalogCheck(): DoctorCheck {
  if (!hasOfflineCatalog()) {
    return check("catalog", "GDA_CATALOG_MISSING", "warn", "no offline catalog cached", "Run 'gda tool list' against a live instance to cache a hash-verified catalog.");
  }
  try {
    const doc = readOfflineCatalog();
    return check("catalog", "GDA_CATALOG_OK", "ok", `${doc.tools.length} tools, hash ${doc.hash}`, "none");
  } catch (error) {
    return check("catalog", "GDA_CATALOG_HASH_MISMATCH", "error", (error as Error).message, "Remove the cached catalog and run 'gda tool list' again.");
  }
}

export async function doctor(opts: GlobalOptions): Promise<number> {
  const ci = ((opts as unknown as Record<string, unknown>)["--ci"] === true);
  const report: DoctorReport = { schema_version: 1, ok: true, mode: ci ? "ci" : "full", checks: [], summary: { ok: 0, warn: 0, error: 0 } };
  const registry = registryCheck();
  report.checks.push(registry);
  let instances: Array<{ id: string; pid: number; project: string; ipcEndpoint?: { kind?: string; path?: string; name?: string; host?: string; port?: number } | null }> = [];
  if (registry.status !== "error") {
    instances = loadRegistry().instances;
    report.checks.push(...instanceChecks(instances));
    report.checks.push(nativeCheck(instances));
  } else {
    report.checks.push(check("instances", "GDA_INSTANCE_UNKNOWN", "error", "registry unreadable", "Repair the registry first."));
    report.checks.push(check("native", "GDA_NATIVE_UNKNOWN", "error", "registry unreadable", "Repair the registry first."));
  }
  report.checks.push(catalogCheck());
  finalize(report);
  writeJson(report, opts);
  return report.ok ? ExitCode.Success : ExitCode.Transport;
}

export async function diagnose(positional: string[], opts: GlobalOptions): Promise<number> {
  const ci = ((opts as unknown as Record<string, unknown>)["--ci"] === true);
  const report: DoctorReport = { schema_version: 1, ok: true, mode: ci ? "ci" : "full", checks: [], summary: { ok: 0, warn: 0, error: 0 } };
  try {
    const target = resolveNativeTarget(opts, TARGET_CONTEXT);
    report.target = {
      id: target.instanceId ?? target.instance?.id ?? "",
      project: target.pluginRecord?.projectPath ?? target.instance?.project ?? "",
      port: target.port,
      source: target.source,
      pinned: target.pinned === true,
    };
    report.checks.push(check("target", "GDA_TARGET_OK", "ok", `resolved via ${target.chain}`, "none"));
    report.checks.push(check("identity", "GDA_INSTANCE_FRESH", "ok", `pid ${target.instance?.pid ?? target.pluginRecord?.pid ?? 0}`, "none"));
    if (target.ipcEndpoint === undefined) {
      report.checks.push(check("native", "GDA_IPC_ENDPOINT_MISSING", "error", "no native IPC endpoint", "Pass --socket or restart the editor with the plugin enabled."));
    } else {
      report.checks.push(check("native", "GDA_IPC_ENDPOINT_OK", "ok", target.ipcEndpoint.kind ?? "unknown", "none"));
      const identity = target.instanceId ?? target.pluginRecord?.instanceId ?? target.instance?.id ?? "";
      try {
        const hello = await nativeHello(target.ipcEndpoint, identity);
        report.checks.push(check("hello", "GDA_NATIVE_OK", "ok", `instance ${hello.instance_id}`, "none"));
      } catch (error) {
        report.checks.push(check("hello", "GDA_NATIVE_FAILED", "error", (error as Error).message, "Check that the plugin's native dispatcher is enabled; do not fall back to MCP initialize."));
      }
    }
  } catch (error) {
    if (error instanceof CliError) {
      const remediation = error.suggestion === undefined
        ? "Run 'gda editor list' and pass --id, --socket, --port, or --project."
        : `${error.suggestion} Pass --id to pin a specific instance.`;
      report.checks.push(check("target", "GDA_TARGET_FAILED", "error", error.message, remediation));
    } else {
      report.checks.push(check("target", "GDA_TARGET_FAILED", "error", (error as Error).message, "Run 'gda editor list' and retry."));
    }
  }
  finalize(report);
  writeJson(report, opts);
  return report.ok ? ExitCode.Success : ExitCode.Transport;
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0];
  if (sub === undefined || sub === "run") return doctor(opts);
  if (sub === "diagnose") return diagnose(positional.slice(1), opts);
  throw new CliError(ExitCode.Usage, `unknown doctor subcommand: ${sub} (expected run/diagnose)`);
}
