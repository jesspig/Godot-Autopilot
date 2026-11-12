import fs from "node:fs";
import path from "node:path";

import {
  ROOT,
  decodeUtf8Strict,
  isDirectory,
  readBytes,
  relOf,
  walkFiles,
} from "./guard_support";

const SOURCES_DIR = path.join(ROOT, "cmake", "sources");
const PLUGIN_MANIFESTS = [
  "gda_core.cmake",
  "api_core.cmake",
  "api_contract.cmake",
  "api_storage.cmake",
  "api_transport.cmake",
  "api_cli.cmake",
  "api_instance_guard.cmake",
];
const L1_MANIFESTS = [
  "gda_unit_sources.cmake",
  "api_unit_contract.cmake",
  "api_unit_storage.cmake",
  "api_unit_transport.cmake",
  "api_unit_cli.cmake",
  "api_unit_docs.cmake",
  "api_unit_instance_guard.cmake",
  "api_probe.cmake",
];
const BLOCK_PATTERN = /(?:set|add_executable)\s*\(([^)]*)\)/gs;
const VAR_REF_PATTERN = /\$\{[^}]+\}/;
const COMMENT_PATTERN = /#[^\n]*/g;
const CMAKE_SOURCE_PREFIX = "${CMAKE_SOURCE_DIR}/";

class ManifestReadError extends Error {}

function parseManifestTokens(file: string): string[] {
  let text: string;
  try {
    text = decodeUtf8Strict(readBytes(file));
  } catch (error) {
    throw new ManifestReadError(
      `source-manifest-guard: read error: ${relOf(file)}: ${(error as Error).message}`,
    );
  }
  const tokens: string[] = [];
  const withoutComments = text.replace(COMMENT_PATTERN, "");
  for (const block of withoutComments.matchAll(BLOCK_PATTERN)) {
    for (const raw of block[1].split(/\s+/)) {
      if (raw === "") continue;
      let token = raw;
      if (token.startsWith(CMAKE_SOURCE_PREFIX)) token = token.slice(CMAKE_SOURCE_PREFIX.length);
      if (VAR_REF_PATTERN.test(token)) continue;
      tokens.push(token.replace(/\\/g, "/"));
    }
  }
  return tokens;
}

function opsFiles(): string[] {
  const found: string[] = [];
  const domains = path.join(ROOT, "src", "tools", "domains");
  if (isDirectory(domains)) {
    for (const entry of fs.readdirSync(domains, { withFileTypes: true })) {
      if (!entry.isDirectory()) continue;
      const dir = path.join(domains, entry.name);
      for (const file of fs.readdirSync(dir, { withFileTypes: true })) {
        if (file.isFile() && file.name.endsWith("_ops.cpp")) found.push(path.join(dir, file.name));
      }
    }
  }
  const base = path.join(ROOT, "src", "tools", "base");
  if (isDirectory(base)) {
    for (const file of fs.readdirSync(base, { withFileTypes: true })) {
      if (file.isFile() && file.name.endsWith("_ops.cpp")) found.push(path.join(base, file.name));
    }
  }
  return found;
}

function main(): number {
  try {
    const pluginEntries = new Set<string>();
    for (const name of PLUGIN_MANIFESTS) {
      for (const token of parseManifestTokens(path.join(SOURCES_DIR, name))) {
        if (token.startsWith("src/")) pluginEntries.add(token);
      }
    }
    for (const file of opsFiles()) pluginEntries.add(relOf(file));

    const srcActual = new Set(
      walkFiles(path.join(ROOT, "src"))
        .filter((file) => file.endsWith(".cpp"))
        .map((file) => relOf(file)),
    );

    const errors: string[] = [];
    for (const item of [...srcActual].filter((item) => !pluginEntries.has(item)).sort()) {
      errors.push(`plugin manifests miss: ${item}`);
    }
    for (const item of [...pluginEntries].filter((item) => !srcActual.has(item)).sort()) {
      errors.push(`plugin manifests list non-existent source: ${item}`);
    }

    const l1Entries = new Set<string>();
    for (const name of L1_MANIFESTS) {
      for (const token of parseManifestTokens(path.join(SOURCES_DIR, name))) {
        if (token.startsWith("src/")) l1Entries.add(token);
      }
    }
    for (const item of [...l1Entries].filter((item) => !pluginEntries.has(item)).sort()) {
      errors.push(`L1 manifests reference source not in plugin manifests: ${item}`);
    }

    if (errors.length > 0) {
      for (const item of errors) console.error(item);
      return 1;
    }

    console.log(
      `source-manifest-guard plugin sources ${pluginEntries.size} == src/ *.cpp ${srcActual.size}; L1 manifests reference ${l1Entries.size} plugin sources, all registered`,
    );
    console.log("source-manifest-guard OK");
    return 0;
  } catch (error) {
    if (error instanceof ManifestReadError) {
      console.error(error.message);
      return 2;
    }
    throw error;
  }
}

process.exitCode = main();
