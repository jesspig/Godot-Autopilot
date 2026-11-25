import { expect, test } from "bun:test";

import { cliUpdateBinaryName } from "../src/update";

test("cliUpdateBinaryName appends .exe on Windows", () => {
  expect(cliUpdateBinaryName("win32")).toBe("gda.exe");
});

test("cliUpdateBinaryName keeps the extensionless name on POSIX", () => {
  expect(cliUpdateBinaryName("linux")).toBe("gda");
  expect(cliUpdateBinaryName("darwin")).toBe("gda");
});
