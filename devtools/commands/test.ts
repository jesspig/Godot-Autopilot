import { PROJECT_ROOT, resolvePreset } from "../lib/config";

export type TestOptions = {
  preset: string;
  single: string | null;
};

export function runTests(options: TestOptions): number {
  const resolved = resolvePreset(options.preset);
  const command = ["ctest", "--preset", resolved];
  if (options.single !== null && options.single !== "") command.push("-R", options.single);
  console.log(`\n$ ${command.join(" ")}`);
  try {
    const result = Bun.spawnSync(command, {
      cwd: PROJECT_ROOT,
      stdin: "inherit",
      stdout: "inherit",
      stderr: "inherit",
    });
    return result.exitCode;
  } catch (error) {
    console.error(`[ERROR] failed to start ctest: ${error instanceof Error ? error.message : String(error)}`);
    return 1;
  }
}
