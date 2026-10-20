import fs from "node:fs";
import path from "node:path";

import type { GlobalOptions } from "./args";
import { CliError, ExitCode } from "./errors";
import type { JsonRpcResponse } from "./mcp/client";
import { err, renderJson } from "./output";

const DEFAULT_TIMEOUT_SEC = 60;

export interface ExtractOptions {
  saveImages?: string;
  raw?: boolean;
  projectResults?: boolean;
}

export interface ImageReport {
  mimeType: string;
  bytes: number;
  saved: boolean;
  path?: string;
}

export interface ExtractedPayload {
  display: unknown;
  toolFailed: boolean;
  failureMessage?: string;
}

interface RpcErrorInfo {
  code: number | null;
  message: string;
}

function utcStamp(): string {
  return new Date().toISOString().replace(/[-:.]/g, "");
}

function readRpcError(envelope: unknown): RpcErrorInfo | null {
  if (envelope === null || typeof envelope !== "object") return null;
  const raw = (envelope as { error?: unknown }).error;
  if (raw === undefined || raw === null) return null;
  if (typeof raw === "string") return { code: null, message: raw };
  if (typeof raw !== "object" || Array.isArray(raw)) return { code: null, message: "" };
  const record = raw as { code?: unknown; message?: unknown };
  const code = typeof record.code === "number" ? record.code : null;
  const message = typeof record.message === "string" ? record.message : "";
  return { code, message };
}

function formatRpcError(info: RpcErrorInfo): string {
  const text = info.message.trim();
  const body = text === "" ? "the server returned a JSON-RPC error envelope" : text;
  const code = info.code === null ? "" : ` (code ${info.code})`;
  return `tool failure: ${body}${code}`;
}

function hasInnerError(payload: unknown): boolean {
  if (!payload || typeof payload !== "object" || Array.isArray(payload)) return false;
  const record = payload as Record<string, unknown>;
  if ("error" in record) return true;
  const innerResult = record.result !== undefined ? record.result : record.data;
  if (innerResult && typeof innerResult === "object" && !Array.isArray(innerResult) && "error" in innerResult) {
    return true;
  }
  if (Array.isArray(record.results)) {
    return record.results.some(
      (item) =>
        item !== null &&
        typeof item === "object" &&
        (((item as Record<string, unknown>).status === "error") || "error" in item),
    );
  }
  return false;
}

function redactedEnvelope(envelope: unknown): unknown {
  const clone = JSON.parse(JSON.stringify(envelope)) as { result?: { content?: unknown } };
  const content = clone.result?.content;
  if (Array.isArray(content)) {
    for (const item of content) {
      if (
        item !== null &&
        typeof item === "object" &&
        (item as Record<string, unknown>).type === "image" &&
        typeof (item as Record<string, unknown>).data === "string"
      ) {
        const record = item as Record<string, unknown>;
        record.data = `<redacted ${Buffer.from(record.data as string, "base64").length} bytes>`;
      }
    }
  }
  return clone;
}

export function extractPayload(envelope: JsonRpcResponse, opts: ExtractOptions = {}): ExtractedPayload {
  const rpcError = readRpcError(envelope);
  if (rpcError !== null) {
    const display = opts.raw === true ? redactedEnvelope(envelope) : { error: { code: rpcError.code, message: rpcError.message } };
    return { display, toolFailed: true, failureMessage: formatRpcError(rpcError) };
  }

  const result = (envelope as { result?: { content?: unknown; isError?: unknown } }).result;
  const content = result?.content;
  if (!Array.isArray(content) || content.length === 0) {
    throw new CliError(ExitCode.Transport, "the response has no result.content entries.");
  }

  const textPayloads: unknown[] = [];
  const imageReports: ImageReport[] = [];
  let saveDir: string | null = null;
  if (opts.saveImages !== undefined) {
    saveDir = path.resolve(opts.saveImages);
    try {
      fs.mkdirSync(saveDir, { recursive: true });
    } catch {
      throw new CliError(ExitCode.Transport, `cannot create --save-images directory: ${opts.saveImages}`);
    }
  }
  const stamp = utcStamp();
  let imageIndex = 0;
  for (const item of content) {
    if (item === null || typeof item !== "object") continue;
    const record = item as { type?: unknown; text?: unknown; data?: unknown; mimeType?: unknown };
    if (record.type === "text" && typeof record.text === "string") {
      try {
        textPayloads.push(JSON.parse(record.text));
      } catch {
        throw new CliError(ExitCode.Transport, "the business payload is not valid JSON.");
      }
    } else if (record.type === "image" && typeof record.data === "string") {
      const bytes = Buffer.from(record.data, "base64").length;
      const mimeType = typeof record.mimeType === "string" ? record.mimeType : "image/png";
      if (saveDir !== null) {
        const fileName = `gda_${stamp}_${imageIndex}.png`;
        const full = path.join(saveDir, fileName);
        try {
          fs.writeFileSync(full, Buffer.from(record.data, "base64"));
        } catch {
          throw new CliError(ExitCode.Transport, `cannot write image file: ${full}`);
        }
        imageReports.push({ mimeType, bytes, saved: true, path: full });
      } else {
        imageReports.push({ mimeType, bytes, saved: false });
      }
      imageIndex += 1;
    }
  }

  let display: unknown;
  if (textPayloads.length === 1) {
    display = textPayloads[0];
  } else if (textPayloads.length > 1) {
    display = textPayloads;
  } else if (imageReports.length > 0) {
    display = {};
  } else {
    throw new CliError(ExitCode.Transport, "the response carries neither text nor image content.");
  }

  const toolFailed =
    result?.isError === true || (Array.isArray(display) ? display.some(hasInnerError) : hasInnerError(display));

  if (opts.projectResults === true && display !== null && typeof display === "object" && !Array.isArray(display)) {
    const record = display as { results?: unknown; result?: { results?: unknown }; data?: { results?: unknown } };
    for (const candidate of [record.results, record.result?.results, record.data?.results]) {
      if (Array.isArray(candidate)) {
        display = candidate;
        break;
      }
    }
  }

  if (imageReports.length > 0) {
    if (display !== null && typeof display === "object" && !Array.isArray(display)) {
      display = { ...(display as Record<string, unknown>), images: imageReports };
    } else {
      display = { result: display, images: imageReports };
    }
  }

  if (opts.raw === true) {
    display = redactedEnvelope(envelope);
  }

  return { display, toolFailed };
}

export function assertCallFamilyUsage(command: string, opts: GlobalOptions): void {
  if (command !== "search" && (opts.category !== undefined || opts.tagsText !== undefined)) {
    throw new CliError(ExitCode.Usage, "--category and --tags only apply to the search command.");
  }
  if (command !== "call" && (opts.argsJson !== undefined || opts.argsFile !== undefined)) {
    throw new CliError(ExitCode.Usage, "--args and --args-file only apply to the call command.");
  }
}

export function loadToolArguments(opts: GlobalOptions): Record<string, unknown> {
  if (opts.argsJson !== undefined && opts.argsFile !== undefined) {
    throw new CliError(ExitCode.Usage, "use only one of --args and --args-file.");
  }
  let args: unknown = {};
  if (opts.argsJson !== undefined) {
    try {
      args = JSON.parse(opts.argsJson);
    } catch {
      throw new CliError(ExitCode.Usage, "--args is not valid JSON.");
    }
  } else if (opts.argsFile !== undefined) {
    let text: string;
    try {
      text = fs.readFileSync(opts.argsFile, "utf8");
    } catch {
      throw new CliError(ExitCode.Usage, `cannot read --args-file: ${opts.argsFile}`);
    }
    try {
      args = JSON.parse(text);
    } catch {
      throw new CliError(ExitCode.Usage, `--args-file does not contain valid JSON: ${opts.argsFile}`);
    }
  }
  if (args === null || typeof args !== "object" || Array.isArray(args)) {
    throw new CliError(ExitCode.Usage, "tool arguments must be a JSON object.");
  }
  return args as Record<string, unknown>;
}

export function callTimeoutSec(opts: GlobalOptions): number {
  if (opts.timeoutText === undefined) return DEFAULT_TIMEOUT_SEC;
  const n = Number(opts.timeoutText);
  if (!Number.isFinite(n) || n <= 0) {
    throw new CliError(ExitCode.Usage, `--timeout must be a positive number of seconds, got: ${opts.timeoutText}`);
  }
  return n;
}

export function emit(extracted: ExtractedPayload, opts: GlobalOptions): number {
  process.stdout.write(`${renderJson(extracted.display, opts)}\n`);
  if (extracted.toolFailed) {
    err(extracted.failureMessage ?? "tool-level failure reported in the business payload.");
    return ExitCode.ToolFailure;
  }
  return ExitCode.Success;
}
