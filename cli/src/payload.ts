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
}

function utcStamp(): string {
  return new Date().toISOString().replace(/[-:.]/g, "");
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

export function callTimeoutSec(opts: GlobalOptions): number {
  if (opts.timeoutText === undefined) return DEFAULT_TIMEOUT_SEC;
  const n = Number(opts.timeoutText);
  if (!Number.isFinite(n) || n <= 0) {
    throw new CliError(ExitCode.Usage, `--timeout must be a positive number of seconds, got: ${opts.timeoutText}`);
  }
  return n;
}

export function emit(display: unknown, toolFailed: boolean, opts: GlobalOptions): number {
  process.stdout.write(`${renderJson(display, opts)}\n`);
  if (toolFailed) {
    err("tool-level failure reported in the business payload.");
    return ExitCode.ToolFailure;
  }
  return ExitCode.Success;
}
