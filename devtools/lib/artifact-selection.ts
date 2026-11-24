import path from "node:path";

export type ArtifactFailure = "none" | "missing" | "ambiguous" | "wrong_config";

export interface ArtifactVerdict {
  failure: ArtifactFailure;
  selected: string | null;
}

export function isDebugPath(candidate: string): boolean {
  return /(^|[/\\])debug([/\\]|$)/i.test(candidate);
}

export function selectUniqueArtifact(
  candidates: readonly string[],
  expectedName: string,
  preset: "debug" | "release",
): ArtifactVerdict {
  const named = candidates.filter((candidate) => path.basename(candidate) === expectedName);
  if (named.length === 0) return { failure: "missing", selected: null };
  if (named.length > 1) return { failure: "ambiguous", selected: null };
  const selected = named[0];
  if (preset === "release" && isDebugPath(selected)) {
    return { failure: "wrong_config", selected: null };
  }
  return { failure: "none", selected };
}
