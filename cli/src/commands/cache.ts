import type { GlobalOptions } from "../args";
import { cleanCache, cacheInfo, listManifests, type CacheArtifact } from "../cache";
import { CliError, ExitCode } from "../errors";
import { writeJson } from "./json-output";

const ARTIFACTS: readonly CacheArtifact[] = ["cli", "plugin", "skills"];

function artifactFilter(opts: GlobalOptions): CacheArtifact | null {
  if (opts.artifactText === undefined) return null;
  if (!ARTIFACTS.includes(opts.artifactText as CacheArtifact)) {
    throw new CliError(ExitCode.Usage, `--artifact must be one of ${ARTIFACTS.join(", ")}`);
  }
  return opts.artifactText as CacheArtifact;
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0] ?? "list";
  if (positional.length > 1) {
    throw new CliError(ExitCode.Usage, "cache takes at most one subcommand: list|info|clear");
  }
  if (sub === "list") {
    const filter = artifactFilter(opts);
    const manifests = listManifests().filter(
      (manifest) => filter === null || manifest.artifact === filter,
    );
    writeJson({ root: cacheInfo().root, manifests }, opts);
    return ExitCode.Success;
  }
  if (sub === "info") {
    writeJson(cacheInfo(), opts);
    return ExitCode.Success;
  }
  if (sub === "clear" || sub === "clean") {
    if (opts.force !== true) {
      throw new CliError(ExitCode.Usage, "cache clear requires --force");
    }
    writeJson(cleanCache(), opts);
    return ExitCode.Success;
  }
  throw new CliError(ExitCode.Usage, `unknown cache subcommand: ${sub}`);
}
