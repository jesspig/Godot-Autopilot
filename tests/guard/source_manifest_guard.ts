import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import {
  ROOT,
  decodeUtf8Strict,
  readBytes,
  toPosix,
  walkFiles,
} from "./guard_support";

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
const MANIFEST_BLOCK_PATTERN = /(?:set|add_executable)\s*\(([^)]*)\)/gs;
const LIBRARY_BLOCK_PATTERN = /add_library\s*\(([^)]*)\)/gs;
const VAR_REF_PATTERN = /\$\{[^}]+\}/;
const COMMENT_PATTERN = /#[^\n]*/g;
const CMAKE_SOURCE_PREFIX = "${CMAKE_SOURCE_DIR}/";

class ManifestReadError extends Error {}

type ScanResult = {
  productionEntries: Set<string>;
  srcActual: Set<string>;
  l1Entries: Set<string>;
  errors: string[];
};

function parseBlockTokens(file: string, pattern: RegExp): string[] {
  let text: string;
  try {
    text = decodeUtf8Strict(readBytes(file));
  } catch (error) {
    throw new ManifestReadError(
      `source-manifest-guard: read error: ${toPosix(path.relative(ROOT, file))}: ${(error as Error).message}`,
    );
  }
  const tokens: string[] = [];
  const withoutComments = text.replace(COMMENT_PATTERN, "");
  for (const block of withoutComments.matchAll(pattern)) {
    for (const raw of block[1].split(/\s+/)) {
      if (raw === "") continue;
      let token = raw;
      if (
        (token.startsWith('"') && token.endsWith('"')) ||
        (token.startsWith("'") && token.endsWith("'"))
      ) {
        token = token.slice(1, -1);
      }
      if (token.startsWith(CMAKE_SOURCE_PREFIX)) token = token.slice(CMAKE_SOURCE_PREFIX.length);
      if (VAR_REF_PATTERN.test(token)) continue;
      tokens.push(token.replace(/\\/g, "/"));
    }
  }
  return tokens;
}

function parseManifestTokens(file: string): string[] {
  return parseBlockTokens(file, MANIFEST_BLOCK_PATTERN);
}

function relativeSource(root: string, file: string): string {
  return toPosix(path.relative(root, file));
}

function scanRoot(root: string): ScanResult {
  const productionEntries = new Set<string>();
  for (const token of parseBlockTokens(path.join(root, "CMakeLists.txt"), LIBRARY_BLOCK_PATTERN)) {
    if (token.startsWith("src/")) productionEntries.add(token);
  }
  for (const name of PLUGIN_MANIFESTS) {
    for (const token of parseManifestTokens(path.join(root, "cmake", "sources", name))) {
      if (token.startsWith("src/")) productionEntries.add(token);
    }
  }

  const srcActual = new Set(
    walkFiles(path.join(root, "src"))
      .filter((file) => file.endsWith(".cpp"))
      .map((file) => relativeSource(root, file)),
  );

  const errors: string[] = [];
  for (const item of [...srcActual].filter((item) => !productionEntries.has(item)).sort()) {
    errors.push(`production manifests miss: ${item}`);
  }
  for (const item of [...productionEntries].filter((item) => !srcActual.has(item)).sort()) {
    errors.push(`production manifests list non-existent source: ${item}`);
  }

  const l1Entries = new Set<string>();
  for (const name of L1_MANIFESTS) {
    for (const token of parseManifestTokens(path.join(root, "cmake", "sources", name))) {
      if (token.startsWith("src/")) l1Entries.add(token);
    }
  }
  for (const item of [...l1Entries].filter((item) => !productionEntries.has(item)).sort()) {
    errors.push(`L1 manifests reference source not in production manifests: ${item}`);
  }

  return { productionEntries, srcActual, l1Entries, errors };
}

function report(result: ScanResult): number {
  if (result.errors.length > 0) {
    for (const item of result.errors) console.error(item);
    return 1;
  }

  console.log(
    `source-manifest-guard production sources ${result.productionEntries.size} == src/ *.cpp ${result.srcActual.size}; L1 manifests reference ${result.l1Entries.size} production sources, all registered`,
  );
  console.log("source-manifest-guard OK");
  return 0;
}

function writeSample(root: string, relative: string, content: string): void {
  const target = path.join(root, relative);
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.writeFileSync(target, content, "utf8");
}

function writeFixtureManifests(root: string): void {
  for (const name of [...PLUGIN_MANIFESTS, ...L1_MANIFESTS]) {
    writeSample(root, path.join("cmake", "sources", name), "set(FIXTURE_SOURCES)\n");
  }
}

function runSelfTests(): number {
  const repository = scanRoot(ROOT);
  if (repository.errors.length > 0) {
    for (const item of repository.errors) console.error(item);
    console.error("source-manifest-guard self-test FAIL: repository scan is not clean");
    return 1;
  }

  const root = fs.mkdtempSync(path.join(os.tmpdir(), "gda-source-manifest-"));
  try {
    writeSample(
      root,
      "CMakeLists.txt",
      "add_library(godot-autopilot SHARED\n    src/core.cpp\n    src/tools/domains/direct/direct_ops.cpp)\n",
    );
    writeFixtureManifests(root);
    writeSample(
      root,
      path.join("cmake", "sources", "gda_core.cmake"),
      "set(GDA_CORE_SOURCES\n    src/tools/domains/listed/listed_ops.cpp)\n",
    );
    writeSample(
      root,
      path.join("cmake", "sources", "gda_unit_sources.cmake"),
      "set(GDA_UNIT_BUSINESS_SOURCES\n    src/core.cpp)\n",
    );
    for (const file of [
      "src/core.cpp",
      "src/tools/domains/direct/direct_ops.cpp",
      "src/tools/domains/listed/listed_ops.cpp",
    ]) {
      writeSample(root, file, "");
    }

    const clean = scanRoot(root);
    if (clean.errors.length > 0) {
      for (const item of clean.errors) console.error(item);
      console.error("source-manifest-guard self-test FAIL: explicitly registered fixture was rejected");
      return 1;
    }

    const unlisted = "src/tools/domains/unlisted/unlisted_ops.cpp";
    writeSample(root, unlisted, "");
    const rejected = scanRoot(root);
    const expected = `production manifests miss: ${unlisted}`;
    if (!rejected.errors.includes(expected)) {
      for (const item of rejected.errors) console.error(item);
      console.error(`source-manifest-guard self-test FAIL: missing rejection: ${expected}`);
      return 1;
    }

    console.log(`negative fixture rejected: ${expected}`);
    console.log("source-manifest-guard self-test OK");
    return 0;
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
}

function main(): number {
  const args = process.argv.slice(2);
  try {
    if (args.length === 1 && args[0] === "--self-test") return runSelfTests();
    let root = ROOT;
    if (args.length === 2 && args[0] === "--root") {
      root = path.resolve(args[1]);
    } else if (args.length > 0) {
      console.error("usage: source_manifest_guard.ts [--root <dir>] [--self-test]");
      return 2;
    }
    if (!fs.existsSync(root) || !fs.statSync(root).isDirectory()) {
      console.error(`source-manifest-guard: root is not a directory: ${root}`);
      return 2;
    }
    return report(scanRoot(root));
  } catch (error) {
    if (error instanceof ManifestReadError) {
      console.error(error.message);
      return 2;
    }
    throw error;
  }
}

process.exitCode = main();
