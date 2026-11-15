import fs from "node:fs";
import path from "node:path";

import { writeBucket } from "../../../src/docs/store";

const home = process.argv[2];
if (home === undefined) {
  throw new Error("missing GODOT_AUTOPILOT_HOME argument");
}
process.env.GODOT_AUTOPILOT_HOME = home;
const text = fs.readFileSync(path.join(import.meta.dir, "extension_api.json"), "utf8");
writeBucket("4.7", text, {
  schema: 1,
  key: "4.7",
  engine: {
    major: 4,
    minor: 7,
    patch: 2,
    status: "stable",
    full_name: "Godot Engine v4.7.2.stable.official",
  },
  source_binary: "worker",
  synced_at: new Date(0).toISOString(),
  bytes: 0,
  sha256: "",
});
