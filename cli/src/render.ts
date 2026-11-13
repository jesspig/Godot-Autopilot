import fs from "node:fs";
import path from "node:path";

import { CliError, ExitCode } from "./errors";

export interface ResourceContent {
  uri: string;
  mimeType?: unknown;
  text?: unknown;
  blob?: unknown;
}

export interface SavedResource {
  uri: string;
  path: string;
  bytes: number;
}

function mimeOf(content: ResourceContent): string {
  return typeof content.mimeType === "string" ? content.mimeType : "";
}

function isJsonMime(mime: string): boolean {
  return mime === "application/json" || mime.endsWith("+json");
}

function textOf(content: ResourceContent): string {
  return typeof content.text === "string" ? content.text : "";
}

export function redactContents(contents: ResourceContent[]): Array<Record<string, unknown>> {
  return contents.map((content) => {
    const record: Record<string, unknown> = { uri: content.uri };
    if (content.mimeType !== undefined) record.mimeType = content.mimeType;
    if (typeof content.text === "string") record.text = content.text;
    if (typeof content.blob === "string") {
      const bytes = Buffer.from(content.blob, "base64").length;
      record.blob = `<blob ${bytes} bytes, ${mimeOf(content) || "application/octet-stream"}>`;
    }
    return record;
  });
}

export function renderTextContents(contents: ResourceContent[], pretty: boolean): string {
  const blocks: string[] = [];
  for (const content of contents) {
    const mime = mimeOf(content);
    if (typeof content.text === "string") {
      if (isJsonMime(mime)) {
        try {
          blocks.push(JSON.stringify(JSON.parse(content.text), null, 2));
        } catch {
          blocks.push(content.text);
        }
      } else {
        blocks.push(content.text);
      }
      continue;
    }
    if (typeof content.blob === "string") {
      const bytes = Buffer.from(content.blob, "base64").length;
      blocks.push(`<blob ${bytes} bytes, ${mime || "application/octet-stream"}>`);
      continue;
    }
    blocks.push(`<empty resource ${content.uri}>`);
  }
  return blocks.join("\n---\n");
}

function safeFileName(uri: string, index: number, mime: string): string {
  let base = "";
  try {
    base = path.basename(new URL(uri).pathname);
  } catch {
    base = path.basename(uri);
  }
  base = base.replace(/[^A-Za-z0-9._-]/g, "_");
  if (base === "" || base === "." || base === "..") {
    const ext = mime === "application/json" ? "json" : "bin";
    base = `resource-${index}.${ext}`;
  }
  return base;
}

export function saveContents(contents: ResourceContent[], dir: string): SavedResource[] {
  const root = path.resolve(dir);
  fs.mkdirSync(root, { recursive: true });
  const saved: SavedResource[] = [];
  contents.forEach((content, index) => {
    if (typeof content.blob !== "string") return;
    const target = path.resolve(root, safeFileName(content.uri, index, mimeOf(content)));
    if (target !== root && !target.startsWith(`${root}${path.sep}`)) {
      throw new CliError(ExitCode.Usage, `refusing to write outside --save directory: ${content.uri}`);
    }
    let bytes: Buffer;
    try {
      bytes = Buffer.from(content.blob, "base64");
    } catch {
      throw new CliError(ExitCode.ToolFailure, `invalid base64 blob for resource: ${content.uri}`);
    }
    fs.writeFileSync(target, bytes);
    saved.push({ uri: content.uri, path: target, bytes: bytes.length });
  });
  return saved;
}
