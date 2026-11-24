import path from "node:path";

export type ArtifactFailure = "none" | "missing" | "ambiguous" | "wrong_config" | "symbol_file";

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
  const symbolBase = expectedName.replace(/\.[^.]+$/, "");
  const symbolFiles = candidates.filter((candidate) =>
    candidate.toLowerCase().endsWith(".pdb") &&
    path.basename(candidate).startsWith(symbolBase),
  );
  const named = candidates.filter((candidate) => path.basename(candidate) === expectedName);
  if (named.length === 0) return { failure: "missing", selected: null };
  if (symbolFiles.length > 0) return { failure: "symbol_file", selected: null };
  if (named.length > 1) return { failure: "ambiguous", selected: null };
  const selected = named[0];
  if (preset === "release" && isDebugPath(selected)) {
    return { failure: "wrong_config", selected: null };
  }
  return { failure: "none", selected };
}
