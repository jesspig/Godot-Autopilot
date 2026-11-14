import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { createHash } from "node:crypto";
import fs from "node:fs";
import path from "node:path";

import { downloadAsset } from "../src/source";

let tmp: string;
let previousGithub: string | undefined;

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmp = fs.mkdtempSync(path.join(base, "gda-source-"));
  previousGithub = process.env.GDA_GITHUB_BASE;
});

afterEach(() => {
  if (previousGithub === undefined) delete process.env.GDA_GITHUB_BASE;
  else process.env.GDA_GITHUB_BASE = previousGithub;
  fs.rmSync(tmp, { recursive: true, force: true });
});

function startServer(body: Buffer, digest: string): { server: ReturnType<typeof Bun.serve>; base: string } {
  const server = Bun.serve({
    port: 0,
    hostname: "127.0.0.1",
    fetch: (request) => {
      const url = new URL(request.url);
      if (url.pathname.endsWith("SHA256SUMS")) {
        return new Response(`gda-0.3.2-windows-x64.zip  ${digest}\n`, {
          headers: { "content-type": "text/plain" },
        });
      }
      if (url.pathname.includes("gda-0.3.2-windows-x64.zip")) {
        return new Response(new Uint8Array(body), { headers: { "content-type": "application/octet-stream" } });
      }
      return new Response("not found", { status: 404 });
    },
  });
  return { server, base: `http://127.0.0.1:${server.port}` };
}

describe("asset source", () => {
  test("从 GitHub 下载并校验", async () => {
    const body = Buffer.from("asset-bytes");
    const digest = createHash("sha256").update(body).digest("hex");
    const good = startServer(body, digest);
    process.env.GDA_GITHUB_BASE = good.base;
    try {
      const downloaded = await downloadAsset("gda-0.3.2-windows-x64.zip", "0.3.2", tmp);
      expect(downloaded.source).toBe("github");
      expect(fs.readFileSync(downloaded.path, "utf8")).toBe("asset-bytes");
    } finally {
      good.server.stop(true);
    }
  });

  test("校验和不一致时拒绝", async () => {
    const body = Buffer.from("asset-bytes");
    const good = startServer(body, "0".repeat(64));
    process.env.GDA_GITHUB_BASE = good.base;
    try {
      await expect(downloadAsset("gda-0.3.2-windows-x64.zip", "0.3.2", tmp)).rejects.toThrow(/checksum mismatch/);
    } finally {
      good.server.stop(true);
    }
  });
});
