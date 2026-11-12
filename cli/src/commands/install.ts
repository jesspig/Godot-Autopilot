import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import type { GlobalOptions } from "../args";
import { extractZip, readTree, writeTree, downloadToFile } from "../archive";
import { cacheRoot, cacheAssetPath, selectLatest, verifyManifest } from "../cache";
import { CliError, ExitCode } from "../errors";
import { readPluginInstanceIndex } from "../instances";
import { isAlive, stopForce } from "../process";
import { builtinSkillsFiles, builtinSkillsVersion } from "../skills";
import { CLI_VERSION } from "../version";
import { start } from "./start";
import { writeJson } from "./json-output";

type Source = "auto" | "online" | "cache" | "builtin";

function parseSource(text: string | undefined): Source {
  if (text === undefined) return "auto";
  if (text === "auto" || text === "online" || text === "cache" || text === "builtin") {
    return text;
  }
  throw new CliError(ExitCode.Usage, `--source must be auto, online, cache, or builtin, got: ${text}`);
}

function skillsArchiveUrl(lang: string): string {
  return `https://github.com/jesspig/Godot-Autopilot/releases/download/v${CLI_VERSION}/skills-${lang}.zip`;
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

async function onlineSkills(lang: string): Promise<Record<string, string>> {
  const temp = fs.mkdtempSync(path.join(cacheRoot(), "skills-online-"));
  try {
    const zip = path.join(temp, "skills.zip");
    await downloadToFile(skillsArchiveUrl(lang), zip);
    const dest = path.join(temp, "unpacked");
    extractZip(zip, dest);
    return normalizeSkillFiles(readTree(dest));
  } finally {
    fs.rmSync(temp, { recursive: true, force: true });
  }
}

function cacheSkills(lang: string): Record<string, string> {
  const manifest = selectLatest("skills", lang);
  if (manifest === undefined) {
    throw new CliError(ExitCode.Transport, `no cached ${lang} skills archive is available.`);
  }
  if (!verifyManifest(manifest)) {
    throw new CliError(ExitCode.Transport, `cached skills archive failed integrity check: ${manifest.asset}`);
  }
  const zip = cacheAssetPath(manifest);
  const temp = fs.mkdtempSync(path.join(cacheRoot(), "skills-cache-"));
  try {
    const dest = path.join(temp, "unpacked");
    extractZip(zip, dest);
    return normalizeSkillFiles(readTree(dest));
  } finally {
    fs.rmSync(temp, { recursive: true, force: true });
  }
}

function builtinSkills(lang: string): Record<string, string> {
  if (lang !== "en") {
    throw new CliError(
      ExitCode.Transport,
      `the builtin skills archive only contains English content (version ${builtinSkillsVersion()}).`,
    );
  }
  return builtinSkillsFiles();
}

async function installSkills(opts: GlobalOptions): Promise<number> {
  const source = parseSource(opts.sourceText);
  const lang = opts.zh === true ? "zh" : "en";
  const target = opts.pathText !== undefined
    ? path.resolve(opts.pathText)
    : path.join(os.homedir(), ".agents", "skills");

  let files: Record<string, string> | undefined;
  let origin: Source = source;
  const tryOnline = async (): Promise<void> => {
    try {
      files = await onlineSkills(lang);
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
  if (files === undefined && (source === "cache" || source === "auto")) {
    try {
      files = cacheSkills(lang);
      origin = "cache";
    } catch (error) {
      if (source === "cache") throw error;
    }
  }
  if (files === undefined && (source === "builtin" || source === "auto")) {
    files = builtinSkills(lang);
    origin = "builtin";
  }
  if (files === undefined) {
    throw new CliError(ExitCode.Transport, "no skills source is available (online, cache, builtin).");
  }
  writeTree(target, files, opts.force === true);
  writeJson(
    {
      ok: true,
      artifact: "skills",
      version: CLI_VERSION,
      origin,
      lang,
      target,
      files: Object.keys(files).length,
      skills: [...new Set(Object.keys(files).map((file) => file.split("/")[0]))].sort(),
    },
    opts,
  );
  return ExitCode.Success;
}

function pluginArchiveUrl(): string {
  return `https://github.com/jesspig/Godot-Autopilot/releases/download/v${CLI_VERSION}/addons.zip`;
}

async function installPlugin(opts: GlobalOptions): Promise<number> {
  const source = parseSource(opts.sourceText);
  const project = path.resolve(opts.project ?? process.cwd());
  const target = path.join(project, "addons", "godot-autopilot");
  const running = readPluginInstanceIndex().filter(
    (record) => record.projectPath === project && record.pid !== null && isAlive(record.pid),
  );
  for (const record of running) {
    if (record.pid !== null) stopForce(record.pid);
  }
  const temp = fs.mkdtempSync(path.join(cacheRoot(), "plugin-install-"));
  try {
    let zip: string | undefined;
    let origin: Source = source;
    if (source === "online" || source === "auto") {
      try {
        zip = path.join(temp, "addons.zip");
        await downloadToFile(pluginArchiveUrl(), zip);
        origin = "online";
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
