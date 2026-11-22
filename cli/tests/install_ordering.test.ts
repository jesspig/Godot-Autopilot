import { describe, expect, test } from "bun:test";

import { shouldStopRunningForInstall } from "../src/commands/install";

describe("install plugin process shutdown ordering", () => {
  test("does not stop the editor before a valid archive is ready", () => {
    expect(shouldStopRunningForInstall(false)).toBe(false);
  });

  test("stops the editor only after the archive is validated", () => {
    expect(shouldStopRunningForInstall(true)).toBe(true);
  });
});
