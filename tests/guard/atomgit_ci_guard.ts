import fs from "node:fs";
import path from "node:path";

const root = path.resolve(import.meta.dir, "..", "..");
const workflowPath = path.join(root, ".gitcode", "workflows", "ci.yml");

function fail(message: string): never {
  console.error(`atomgit-ci-guard: ${message}`);
  process.exit(1);
}

if (!fs.existsSync(workflowPath)) {
  fail("missing .gitcode/workflows/ci.yml");
}

const text = fs.readFileSync(workflowPath, "utf8");
const required = [
  "pull_request:",
  "workflow_dispatch:",
  "stages:",
  "runs-on: [ubuntu-latest, x64, medium]",
  "uses: checkout",
  "bun.sh/install",
  "$ATOMGIT_PATH",
  "git submodule update --init --recursive",
  "cmake --preset debug",
  "cmake --build --preset debug",
  "ctest --preset debug",
  "bun test cli/tests",
  "bun run typecheck",
  "tests/guard/no_python_guard.ts",
];

const missing = required.filter((needle) => !text.includes(needle));
if (missing.length > 0) {
  fail(`workflow is missing required steps: ${missing.join(", ")}`);
}

if (text.includes("windows-2022") || text.includes("macos-latest")) {
  fail("AtomGit hosted runners are Linux-only; do not claim Windows/macOS coverage");
}

console.log("atomgit-ci-guard: OK");
