import { mkdirSync, writeFileSync } from "node:fs";
import path from "node:path";

import { buildManifest } from "../src/command-spec";

const target = path.resolve(import.meta.dir, "../assets/command-manifest.json");
const manifest = buildManifest();

mkdirSync(path.dirname(target), { recursive: true });
writeFileSync(target, `${JSON.stringify(manifest, null, 2)}\n`, "utf8");
process.stdout.write(`wrote ${target} (${manifest.command_count} commands, ${manifest.group_count} groups)\n`);
