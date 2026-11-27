import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import type { GlobalOptions } from "../args";
import { extractZip, readTree } from "../archive";
import { cacheRoot, cacheAssetPath, selectLatest, verifyManifest } from "../cache";
import { CliError, ExitCode } from "../errors";
import { readPluginInstanceIndex } from "../instances";
import { isAlive, terminateGraceful } from "../process";
import {
  applySkillSync,
  builtinSkillsDigest,
  builtinSkillsFiles,
  builtinSkillsVersion,
  planSkillSync,
  skillTreeDigest,
  verifyBuiltinSkillsAsset,
} from "../skills";
import { downloadAsset } from "../source";
import { CLI_VERSION } from "../version";
import { start } from "./start";
import { writeJson } from "./json-output";

type Source = "auto" | "online" | "cache" | "builtin" | "offline";

export function shouldStopRunningForInstall(archiveReady: boolean): boolean {
  return archiveReady;
}

function parseSource(text: string | undefined): Source {
  if (text === undefined) return "auto";
  if (text === "auto" || text === "online" || text === "cache" || text === "builtin") {
    return text;
  }
  throw new CliError(ExitCode.Usage, `--source must be auto, online, cache, or builtin, got: ${text}`);
}

function skillsArchiveUrl(): string {
  return "skills.zip";
}

function normalizeSkillFiles(files: Record<string, string>): Record<string, string> {
  const out: Record<string, string> = {};
  for (const [relative, content] of Object.entries(files)) {
    const stripped = relative.startsWith("skills/") ? relative.slice("skills/".length) : relative;
    if (stripped === "manifest.json" || stripped === "") continue;
    if (!stripped.includes("/")) continue;
    out[stripped] = content;
  }
  return out;
}

async function onlineSkills(): Promise<Record<string, string>> {
  const temp = fs.mkdtempSync(path.join(cacheRoot(), "skills-online-"));
  try {
    const downloaded = await downloadAsset(skillsArchiveUrl(), CLI_VERSION, temp);
    const dest = path.join(temp, "unpacked");
    extractZip(downloaded.path, dest);
    return normalizeSkillFiles(readTree(dest));
  } finally {
    fs.rmSync(temp, { recursive: true, force: true });
  }
}

function cacheSkills(): { files: Record<string, string>; version: string } {
  const manifest = selectLatest("skills");
  if (manifest === undefined) {
    throw new CliError(ExitCode.Transport, "no cached skills archive is available.");
  }
  if (!verifyManifest(manifest)) {
    throw new CliError(ExitCode.Transport, `cached skills archive failed integrity check: ${manifest.asset}`);
  }
  const zip = cacheAssetPath(manifest);
  const temp = fs.mkdtempSync(path.join(cacheRoot(), "skills-cache-"));
  try {
    const dest = path.join(temp, "unpacked");
    extractZip(zip, dest);
    return { files: normalizeSkillFiles(readTree(dest)), version: manifest.version };
  } finally {
    fs.rmSync(temp, { recursive: true, force: true });
  }
}

function builtinSkills(): Record<string, string> {
  try {
    return builtinSkillsFiles();
  } catch (error) {
    throw new CliError(
      ExitCode.Transport,
      `the builtin skills archive is unavailable (version ${builtinSkillsVersion()}).`,
      "Use --source online or --source cache.",
    );
  }
}

async function installSkills(opts: GlobalOptions): Promise<number> {
  const source = opts.offline === true ? "offline" : parseSource(opts.sourceText);
  const target = opts.pathText !== undefined
    ? path.resolve(opts.pathText)
    : path.join(os.homedir(), ".agents", "skills");

  if (!verifyBuiltinSkillsAsset()) {
    throw new CliError(
      ExitCode.Transport,
      "the builtin skills asset digest does not match its source trees",
      "Repair cli/assets/skills.json with build:skills-asset, then reinstall the CLI.",
    );
  }

  let files: Record<string, string> | undefined;
  let origin: Source = source;
  let cacheVersion: string | undefined;
  const tryOnline = async (): Promise<void> => {
    try {
      files = await onlineSkills();
      origin = "online";
    } catch (error) {
      if (source === "online" || source === "auto") {
        if (source === "online") throw error;
      } else {
        throw error;
      }
    }
  };
  if (source === "online" || source === "auto") {
    await tryOnline();
  }
  if (files === undefined && (source === "cache" || source === "auto" || source === "offline")) {
    try {
      const cached = cacheSkills();
      files = cached.files;
      cacheVersion = cached.version;
      origin = "cache";
    } catch (error) {
      if (source === "cache") throw error;
    }
  }
  if (files === undefined && (source === "builtin" || source === "auto" || source === "offline")) {
    files = builtinSkills();
    origin = "builtin";
  }
  if (files === undefined) {
    throw new CliError(ExitCode.Transport, "no skills source is available (online, cache, builtin).");
  }

  const assetVersion = builtinSkillsVersion();
  const assetDigest = builtinSkillsDigest();
  const version = origin === "cache" ? cacheVersion ?? assetVersion : assetVersion;
  const digest = origin === "builtin" || version === assetVersion
    ? assetDigest
    : skillTreeDigest(files);
  const plan = planSkillSync(target, files, {
    version,
    content_digest: digest,
    force: opts.force === true,
    prune: opts.prune === true,
  });
  const applied = applySkillSync(plan);
  if (!applied.ok) {
    throw new CliError(
      ExitCode.Transport,
      `skills sync failed: ${applied.error}`,
      applied.rolled_back
        ? "The previous installation was preserved. Pass --force to overwrite managed files."
        : `Run 'gda skills sync --force' to repair (${applied.error}).`,
    );
  }
  writeJson(
    {
      ok: true,
      artifact: "skills",
      version,
      asset_version: assetVersion,
      content_digest: digest,
      tree_digest: skillTreeDigest(files),
      origin,
      target,
      files: Object.keys(files).length,
      skills: [...new Set(Object.keys(files).map((file) => file.split("/")[0]))].sort(),
      stale: plan.retired_present,
      removed: applied.retired_removed,
      preserved: plan.preserved,
      cache_version: cacheVersion ?? null,
      cache_version_matches_asset: cacheVersion === undefined ? null : cacheVersion === assetVersion,
      sync: {
        written: applied.written,
        deleted: applied.deleted,
        rolled_back: applied.rolled_back,
      },
    },
    opts,
  );
  return ExitCode.Success;
}

interface CsharpRuntimeResult {
  status: "configured" | "already_configured" | "skipped";
  reason: string;
}

export function configureCsharpRuntime(project: string): CsharpRuntimeResult {
  const projectPath = path.join(project, "project.godot");
  if (!fs.existsSync(projectPath)) return { status: "skipped", reason: "no_project_godot" };
  const original = fs.readFileSync(projectPath, "utf8");
  const eol = original.includes("\r\n") ? "\r\n" : "\n";
  const hasProjectFile = fs
    .readdirSync(project)
    .some((entry) => entry.endsWith(".csproj") || entry.endsWith(".sln"));
  const hasDotnetSetting = /\[dotnet\]/i.test(original) && /assembly_name\s*=/i.test(original);
  const hasCsharpFeature = original.includes('"C#"');
  if (!hasProjectFile && !hasDotnetSetting && !hasCsharpFeature) {
    return { status: "skipped", reason: "gdscript_project" };
  }
  const runtimeScript = path.join(project, "addons", "godot-autopilot", "GdaCSharpRuntime.cs");
  if (!fs.existsSync(runtimeScript)) return { status: "skipped", reason: "addon_missing" };
  const lines = original.split(/\r?\n/);
  if (lines.some((line) => /^GdaCSharpRuntime\s*=/i.test(line.trim()))) {
    return { status: "already_configured", reason: "autoload_present" };
  }
  const autoloadIndex = lines.findIndex((line) => /^\[autoload\]/i.test(line.trim()));
  const autoloadLine = 'GdaCSharpRuntime="*res://addons/godot-autopilot/GdaCSharpRuntime.cs"';
  if (autoloadIndex >= 0) {
    lines.splice(autoloadIndex + 1, 0, autoloadLine);
  } else {
    lines.push("", "[autoload]", autoloadLine);
  }
  const temp = `${projectPath}.gda-tmp`;
  fs.writeFileSync(temp, lines.join(eol), "utf8");
  try {
    fs.renameSync(temp, projectPath);
  } catch (error) {
    fs.rmSync(temp, { force: true });
    throw error;
  }
  return { status: "configured", reason: "autoload_added" };
}

async function installPlugin(opts: GlobalOptions): Promise<number> {
  const source = opts.offline === true ? "cache" : parseSource(opts.sourceText);
  const project = path.resolve(opts.project ?? process.cwd());
  const target = path.join(project, "addons", "godot-autopilot");
  const running = readPluginInstanceIndex().filter(
    (record) => record.projectPath === project && record.pid !== null && isAlive(record.pid),
  );
  const temp = fs.mkdtempSync(path.join(cacheRoot(), "plugin-install-"));
  try {
    let zip: string | undefined;
    let origin: string = source;
    if (source === "online" || source === "auto") {
      try {
        const downloaded = await downloadAsset("addons.zip", CLI_VERSION, temp);
        zip = downloaded.path;
        origin = downloaded.source;
      } catch (error) {
        if (source === "online") throw error;
        zip = undefined;
      }
    }
    if (zip === undefined && (source === "cache" || source === "auto")) {
      const manifest = selectLatest("plugin");
      if (manifest !== undefined && verifyManifest(manifest)) {
        zip = cacheAssetPath(manifest);
        origin = "cache";
      } else if (source === "cache") {
        throw new CliError(ExitCode.Transport, "no valid cached plugin archive is available.");
      }
    }
    if (zip === undefined) {
      throw new CliError(
        ExitCode.Transport,
        "the builtin plugin archive is not embedded in this build.",
        "Use --source online/cache, or run the release packaging pipeline that embeds addons.zip.",
      );
    }
    const unpacked = path.join(temp, "unpacked");
    extractZip(zip, unpacked);
    const candidates = [
      path.join(unpacked, "addons", "godot-autopilot"),
      path.join(unpacked, "godot-autopilot"),
      unpacked,
    ];
    const sourceDir = candidates.find((candidate) =>
      fs.existsSync(path.join(candidate, "godot-autopilot.gdextension")),
    );
    if (sourceDir === undefined) {
      throw new CliError(ExitCode.Transport, "the plugin archive does not contain godot-autopilot.gdextension.");
    }
    if (shouldStopRunningForInstall(sourceDir !== undefined)) {
      for (const record of running) {
        if (record.pid === null) continue;
        const outcome = await terminateGraceful(record.pid);
        if (outcome === "failed") {
          throw new CliError(
            ExitCode.Transport,
            `editor (pid ${record.pid}) did not exit after a graceful stop; refusing to replace the plugin while it is running`,
            "Close the editor and retry the install.",
          );
        }
      }
    }
    const backup = `${target}.old-${Date.now()}`;
    if (fs.existsSync(target)) {
      fs.renameSync(target, backup);
    }
    try {
      fs.mkdirSync(path.dirname(target), { recursive: true });
      fs.cpSync(sourceDir, target, { recursive: true });
      if (!fs.existsSync(path.join(target, "godot-autopilot.gdextension"))) {
        throw new Error("installed plugin is missing godot-autopilot.gdextension");
      }
    } catch (error) {
      fs.rmSync(target, { recursive: true, force: true });
      if (fs.existsSync(backup)) fs.renameSync(backup, target);
      throw error;
    }
    const csharpRuntime = opts.withCsharpRuntime === true
      ? configureCsharpRuntime(project)
      : { status: "skipped" as const, reason: "not_requested" };
    if (running.length > 0) {
      await start({ ...opts, project, force: false }, { writeOutput: false });
    }
    writeJson(
      {
        ok: true,
        artifact: "plugin",
        version: CLI_VERSION,
        origin,
        target,
        backup: fs.existsSync(backup) ? backup : null,
        restarted: running.length,
        csharp_runtime: csharpRuntime.status,
        csharp_runtime_reason: csharpRuntime.reason,
        csharp_runtime_hint:
          csharpRuntime.status === "skipped" && csharpRuntime.reason !== "gdscript_project"
            ? "gda call update_autoloads --args '{\"action\":\"add\",\"name\":\"GdaCSharpRuntime\",\"path\":\"res://addons/godot-autopilot/GdaCSharpRuntime.cs\",\"enabled\":true}'"
            : null,
      },
      opts,
    );
    return ExitCode.Success;
  } finally {
    fs.rmSync(temp, { recursive: true, force: true });
  }
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const target = positional[0];
  if (target === "skills") return await installSkills(opts);
  if (target === "plugin") return await installPlugin(opts);
  throw new CliError(ExitCode.Usage, "install requires skills or plugin");
}
