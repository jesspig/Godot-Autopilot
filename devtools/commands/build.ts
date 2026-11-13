import { buildPreset, configure } from "../lib/cmake";

export type BuildOptions = {
  release: boolean;
  debug: boolean;
};

export async function runBuild(options: BuildOptions): Promise<number> {
  if (options.release && options.debug) {
    console.log("[ERROR] Cannot specify both --release and --debug");
    return 1;
  }

  const preset = options.release ? "release" : "debug";
  const config = options.release ? "Release" : "Debug";

  console.log(`[BUILD] Config=${config}`);
  if (!(await configure(preset))) return 1;
  if (!(await buildPreset(preset))) return 1;

  console.log("\n[DONE] Build complete.");
  return 0;
}
