import { createHash } from "node:crypto";
import fs from "node:fs";
import path from "node:path";

interface Options {
  dir: string;
  tag: string;
  repo: string;
  api: string;
  token: string;
  notes: string;
  dryRun: boolean;
}

function parseArgs(argv: string[]): Options {
  const env = process.env;
  const options: Options = {
    dir: "dist",
    tag: "",
    repo: env.GDA_ATOMGIT_REPO ?? env.ATOMGIT_REPOSITORY ?? "",
    api: env.GDA_ATOMGIT_API ?? "https://api.atomgit.com/api/v5",
    token: env.GDA_ATOMGIT_TOKEN ?? env.ATOMGIT_TOKEN ?? "",
    notes: "",
    dryRun: false,
  };
  for (let index = 0; index < argv.length; index += 1) {
    const token = argv[index];
    const next = (): string => {
      const value = argv[index + 1];
      if (value === undefined) throw new Error(`${token} requires a value`);
      index += 1;
      return value;
    };
    if (token === "--dir") options.dir = next();
    else if (token === "--tag") options.tag = next();
    else if (token === "--repo") options.repo = next();
    else if (token === "--api") options.api = next();
    else if (token === "--notes") options.notes = next();
    else if (token === "--dry-run") options.dryRun = true;
    else if (token === "--help" || token === "-h") {
      console.log("usage: bun tools/mirror_atomgit.ts --dir dist --tag 0.3.3 --repo owner/repo [--dry-run]");
      process.exit(0);
    } else throw new Error(`unknown option: ${token}`);
  }
  return options;
}

function assetNames(tag: string): string[] {
  const version = tag.replace(/^v/, "");
  return [
    "addons.zip",
    `gda-${version}-windows-x64.zip`,
    `gda-${version}-linux-x64.tar.gz`,
    `gda-${version}-linux-x64-musl.tar.gz`,
    `gda-${version}-macos-arm64.tar.gz`,
    `gda-${version}-macos-x64.tar.gz`,
    "sha256sums.txt",
  ];
}

function sha256(filePath: string): string {
  return createHash("sha256").update(fs.readFileSync(filePath)).digest("hex");
}

async function requestJson(url: string, init: RequestInit): Promise<{ status: number; body: unknown; text: string }> {
  const response = await fetch(url, init);
  const text = await response.text();
  let body: unknown = null;
  try {
    body = text === "" ? null : JSON.parse(text);
  } catch {
    body = text;
  }
  return { status: response.status, body, text };
}

async function ensureRelease(options: Options, version: string): Promise<void> {
  const query = `access_token=${encodeURIComponent(options.token)}`;
  const create = await requestJson(`${options.api}/repos/${options.repo}/releases?${query}`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({
      tag_name: options.tag,
      name: `Godot Autopilot ${version}`,
      body: options.notes === "" ? `AtomGit mirror of Godot Autopilot ${version}` : fs.readFileSync(options.notes, "utf8"),
    }),
  });
  if (create.status >= 200 && create.status < 300) return;
  const existing = await requestJson(
    `${options.api}/repos/${options.repo}/releases/tags/${encodeURIComponent(options.tag)}?${query}`,
    { method: "GET" },
  );
  if (existing.status >= 200 && existing.status < 300) return;
  throw new Error(`cannot create or find AtomGit release (create ${create.status}, get ${existing.status}): ${create.text.slice(0, 300)}`);
}

async function existingAssets(options: Options): Promise<Array<{ id: string | number; name: string }>> {
  const query = `access_token=${encodeURIComponent(options.token)}`;
  const response = await requestJson(
    `${options.api}/repos/${options.repo}/releases/tags/${encodeURIComponent(options.tag)}?${query}`,
    { method: "GET" },
  );
  if (response.status < 200 || response.status >= 300) return [];
  const assets = (response.body as { assets?: unknown } | null)?.assets;
  if (!Array.isArray(assets)) return [];
  return assets
    .filter((item): item is { id: string | number; name: string } =>
      item !== null && typeof item === "object" &&
      typeof (item as { name?: unknown }).name === "string" &&
      (typeof (item as { id?: unknown }).id === "string" || typeof (item as { id?: unknown }).id === "number"))
    .map((item) => ({ id: item.id, name: item.name }));
}

async function upload(options: Options, filePath: string, name: string): Promise<void> {
  const query = `access_token=${encodeURIComponent(options.token)}&file_name=${encodeURIComponent(name)}`;
  const uploadInfo = await requestJson(
    `${options.api}/repos/${options.repo}/releases/${encodeURIComponent(options.tag)}/upload_url?${query}`,
    { method: "GET" },
  );
  if (uploadInfo.status < 200 || uploadInfo.status >= 300) {
    throw new Error(`cannot get upload url for ${name}: ${uploadInfo.status} ${uploadInfo.text.slice(0, 200)}`);
  }
  const info = uploadInfo.body as { url?: unknown; headers?: unknown } | null;
  if (info === null || typeof info.url !== "string") {
    throw new Error(`upload url response is missing url for ${name}`);
  }
  const headers: Record<string, string> = {};
  if (info.headers !== null && typeof info.headers === "object" && !Array.isArray(info.headers)) {
    for (const [key, value] of Object.entries(info.headers as Record<string, unknown>)) {
      if (typeof value === "string") headers[key] = value;
    }
  }
  const response = await fetch(info.url, {
    method: "PUT",
    headers,
    body: new Uint8Array(fs.readFileSync(filePath)),
  });
  if (response.status < 200 || response.status >= 300) {
    throw new Error(`upload failed for ${name}: ${response.status} ${(await response.text()).slice(0, 200)}`);
  }
}

async function main(): Promise<void> {
  const options = parseArgs(process.argv.slice(2));
  if (options.tag === "") throw new Error("--tag is required");
  const version = options.tag.replace(/^v/, "");
  if (!fs.existsSync(options.dir)) throw new Error(`asset directory not found: ${options.dir}`);
  const names = assetNames(options.tag);
  const missing = names.filter((name) => !fs.existsSync(path.join(options.dir, name)));
  if (missing.length > 0) throw new Error(`missing assets: ${missing.join(", ")}`);
  for (const name of names) {
    console.log(`${sha256(path.join(options.dir, name))}  ${name}`);
  }
  if (options.dryRun) {
    console.log(`dry-run: ${names.length} assets for ${options.repo || "<repo>"} tag ${options.tag}`);
    return;
  }
  if (options.repo === "" || options.token === "") {
    throw new Error("GDA_ATOMGIT_REPO and GDA_ATOMGIT_TOKEN (or ATOMGIT_TOKEN) are required for upload");
  }
  await ensureRelease(options, version);
  for (const asset of await existingAssets(options)) {
    if (!names.includes(asset.name)) continue;
    const query = `access_token=${encodeURIComponent(options.token)}`;
    const removed = await requestJson(
      `${options.api}/repos/${options.repo}/releases/${encodeURIComponent(options.tag)}/attach_files/${encodeURIComponent(String(asset.id))}?${query}`,
      { method: "DELETE" },
    );
    if (removed.status >= 300) {
      throw new Error(`cannot replace existing asset ${asset.name}: ${removed.status}`);
    }
  }
  for (const name of names) {
    await upload(options, path.join(options.dir, name), name);
    console.log(`uploaded ${name}`);
  }
  console.log(`mirrored ${names.length} assets to AtomGit ${options.repo} (${options.tag})`);
}

main().catch((error: unknown) => {
  console.error(`atomgit mirror failed: ${(error as Error)?.message ?? String(error)}`);
  process.exit(1);
});
