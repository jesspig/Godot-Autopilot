import fs from "node:fs";
import path from "node:path";

import { describe, expect, test } from "bun:test";

import { BRIDGE_DIR } from "./config";

function bridgeSource(name: string): string {
  return fs.readFileSync(path.join(BRIDGE_DIR, name), "utf8");
}

describe("C# bridge conversion contract", () => {
  const source = bridgeSource("BridgeConvert.cs");

  test("integer vectors round-trip through flat int arrays", () => {
    expect(source).toContain("typeof(Vector2I)");
    expect(source).toContain("typeof(Vector3I)");
    expect(source).toContain("typeof(Vector4I)");
    expect(source).toContain("packed integer vector length is not a multiple of");
    expect(source).toContain("int[] flat");
  });

  test("unsupported variants and return types report an explicit error", () => {
    expect(source).toContain("unsupported packed array variant type");
    expect(source).toContain("unsupported return value type");
    expect(source).toContain("lastConversionError");
  });

  test("exact type matches cost zero and string conversions cost more", () => {
    expect(source).toContain("cost = 2;");
    expect(source.match(/cost = 0;/g)?.length ?? 0).toBeGreaterThan(0);
  });
});

describe("C# bridge event contract", () => {
  const source = bridgeSource("BridgeEvents.cs");

  test("subscribe validates the target method", () => {
    expect(source).toContain("HasMethod(targetMethod)");
    expect(source).toContain("has no callable method");
  });

  test("forwarding failures are recorded and observable", () => {
    expect(source).toContain("GD.PushError");
    expect(source).toContain("subscriptionHealth");
    expect(source).toContain("FailureCount");
    expect(source).toContain("LastError");
  });
});
