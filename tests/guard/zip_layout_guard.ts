import { randomBytes } from "node:crypto";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { readZip, validateZipLayout, writeZip } from "../../devtools/lib/zip";

function fail(message: string): never {
  console.error(`zip-layout-guard: ${message}`);
  process.exit(1);
}

const dir = fs.mkdtempSync(path.join(os.tmpdir(), "gda-zip-layout-"));
const valid = path.join(dir, "valid.zip");
const broken = path.join(dir, "broken.zip");

try {
  const payload = randomBytes(200_000);
  writeZip(valid, [{ name: "payload.bin", data: payload, mtime: new Date() }]);
  const layout = validateZipLayout(valid);
  if (layout !== null) fail(`writeZip produced an invalid layout: ${layout}`);
  const entries = readZip(valid);
  const found = entries.find((entry) => entry.name === "payload.bin");
  if (found === undefined) fail("readZip did not return payload.bin");
  if (!found.data.equals(payload)) fail("readZip payload mismatch");

  const buffer = fs.readFileSync(valid);
  let eocd = -1;
  for (let offset = buffer.length - 22; offset >= 0; offset -= 1) {
    if (buffer.readUInt32LE(offset) === 0x06054b50) {
      eocd = offset;
      break;
    }
  }
  if (eocd < 0) fail("cannot locate EOCD");
  buffer.writeUInt32LE(0, eocd + 16);
  fs.writeFileSync(broken, buffer);
  const brokenLayout = validateZipLayout(broken);
  if (brokenLayout === null) fail("validateZipLayout accepted a broken central directory offset");
} finally {
  fs.rmSync(dir, { recursive: true, force: true });
}

console.log("zip-layout-guard: OK");
