import fs from "node:fs";
import os from "node:os";
import path from "node:path";

const root = path.resolve(import.meta.dir, "..", "..");

function fail(message: string): never {
  console.error(`atomgit-release-guard: ${message}`);
  process.exit(1);
}

const releasePath = path.join(root, ".gitcode", "workflows", "release.yml");
const syncPath = path.join(root, ".github", "workflows", "atomgit-sync.yml");
const toolPath = path.join(root, "tools", "publish_atomgit.ts");
for (const file of [releasePath, syncPath, toolPath]) {
  if (!fs.existsSync(file)) fail(`missing ${file}`);
}

const release = fs.readFileSync(releasePath, "utf8");
for (const needle of [
  "push:",
  "tags:",
  "permissions:",
  "repository: write",
  "bun-windows-x64",
  "bun-linux-x64",
  "bun-linux-x64-musl",
  "bun-darwin-arm64",
  "bun-darwin-x64",
  "tools/publish_atomgit.ts",
  "ATOMGIT_REPOSITORY",
  "ATOMGIT_REF#refs/tags/",
  "https://api.gitcode.com/api/v5",
]) {
  if (!release.includes(needle)) fail(`release workflow is missing ${needle}`);
}
for (const forbidden of ["ATOMGIT_REF_NAME", "ATOMGIT_API_URL"]) {
  if (release.includes(forbidden)) fail(`release workflow uses undocumented variable ${forbidden}`);
}

const sync = fs.readFileSync(syncPath, "utf8");
for (const needle of ["branches:", "master", "ATOMGIT_TOKEN", "ATOMGIT_REPO", "HEAD:master", "--tags"]) {
  if (!sync.includes(needle)) fail(`sync workflow is missing ${needle}`);
}

const tool = fs.readFileSync(toolPath, "utf8");
for (const needle of ["upload_url", "access_token", "method: \"PUT\"", "attach_files", "sha256sums.txt"]) {
  if (!tool.includes(needle)) fail(`publish tool is missing ${needle}`);
}

const tag = "0.3.3";
const dir = fs.mkdtempSync(path.join(os.tmpdir(), "gda-atomgit-"));
const assets = [
  `gda-${tag}-windows-x64.zip`,
  `gda-${tag}-linux-x64.tar.gz`,
  `gda-${tag}-linux-x64-musl.tar.gz`,
  `gda-${tag}-macos-arm64.tar.gz`,
  `gda-${tag}-macos-x64.tar.gz`,
  "sha256sums.txt",
];
try {
  for (const name of assets) fs.writeFileSync(path.join(dir, name), name);
  const run = Bun.spawnSync([process.execPath, toolPath, "--dry-run", "--dir", dir, "--tag", tag, "--repo", "owner/repo"], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (run.exitCode !== 0) {
    fail(`dry-run failed: ${run.stderr.toString()}`);
  }
  const output = run.stdout.toString();
  for (const name of assets) {
    if (!output.includes(name)) fail(`dry-run output is missing ${name}`);
  }
  if (!output.includes("dry-run: 6 assets")) fail("dry-run did not report 6 assets");
} finally {
  fs.rmSync(dir, { recursive: true, force: true });
}

console.log("atomgit-release-guard: OK");
