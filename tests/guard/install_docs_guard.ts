import fs from "node:fs";
import path from "node:path";

const root = path.resolve(import.meta.dir, "..", "..");
const installPath = path.join(root, "INSTALL.md");
const shellPath = path.join(root, "scripts", "install.sh");
const powershellPath = path.join(root, "scripts", "install.ps1");

function fail(message: string): never {
  console.error(`install-docs-guard: ${message}`);
  process.exit(1);
}

for (const file of [installPath, shellPath, powershellPath]) {
  if (!fs.existsSync(file)) fail(`missing ${path.relative(root, file)}`);
}

const install = fs.readFileSync(installPath, "utf8");
for (const needle of [
  "## 中文速览",
  "## For AI agents",
  "## Option A",
  "## Option B",
  "## Option C",
  "## Connect your MCP client",
  "## Verify the installation",
  "## Troubleshooting",
  "scripts/install.sh",
  "scripts/install.ps1",
  "--project",
  "--skills",
  "--skills-project",
  "~/.agents/skills",
  "<project>/.agents/skills",
  "project.godot",
  "mcp_url",
  "sha256sums.txt",
]) {
  if (!install.includes(needle)) fail(`INSTALL.md is missing ${needle}`);
}

const shell = fs.readFileSync(shellPath, "utf8");
for (const needle of [
  "resolve_latest",
  "sha256sums.txt",
  "find_project_root",
  "--project",
  "--skills",
  "--skills-project",
  "--no-plugin",
]) {
  if (!shell.includes(needle)) fail(`install.sh is missing ${needle}`);
}

const powershell = fs.readFileSync(powershellPath, "utf8");
for (const needle of [
  "Resolve-Latest",
  "sha256sums.txt",
  "Find-ProjectRoot",
  "-Project",
  "-Skills",
  "-SkillsProject",
  "-NoPlugin",
]) {
  if (!powershell.includes(needle)) fail(`install.ps1 is missing ${needle}`);
}

for (const [label, text] of [
  ["install.sh", shell],
  ["install.ps1", powershell],
  ["INSTALL.md", install],
] as const) {
  for (const forbidden of ["ATOMGIT", "GDA_ATOMGIT_BASE"]) {
    if (text.includes(forbidden)) fail(`${label} still references ${forbidden}`);
  }
}

console.log("install-docs-guard: OK");
