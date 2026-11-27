import { createHash } from "node:crypto";
import fs from "node:fs";
import path from "node:path";

import { usageText } from "../cli/src/args";
import { readContract } from "../tests/guard/skills_guard";

const root = path.resolve(import.meta.dir, "..");

function collectSkillFiles(relativeRoot: string): Array<{ path: string; bytes: number; sha256: string }> {
  const base = path.join(root, relativeRoot);
  const files: Array<{ path: string; bytes: number; sha256: string }> = [];
  const walk = (directory: string, prefix: string): void => {
    for (const entry of fs.readdirSync(directory, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
      const full = path.join(directory, entry.name);
      const relative = prefix === "" ? entry.name : `${prefix}/${entry.name}`;
      if (entry.isDirectory()) {
        walk(full, relative);
      } else if (entry.isFile()) {
        const bytes = fs.readFileSync(full);
        files.push({
          path: `${relativeRoot}/${relative}`,
          bytes: bytes.length,
          sha256: createHash("sha256").update(bytes).digest("hex"),
        });
      }
    }
  };
  walk(base, "");
  return files;
}

const contract = readContract(root);
const payload = {
  schema: 1,
  captured_from: "pre-change T-01 baseline",
  version: fs.readFileSync(path.join(root, "VERSION"), "utf8").trim(),
  cli: {
    commands: [...contract.commands].sort(),
    flags: [...contract.flags].sort(),
    status_fields: [...contract.statusFields].sort(),
    help: usageText(),
  },
  skills: {
    en: collectSkillFiles("skills"),
    zh: collectSkillFiles("skills-zh"),
  },
};

process.stdout.write(`${JSON.stringify(payload, null, 2)}\n`);
