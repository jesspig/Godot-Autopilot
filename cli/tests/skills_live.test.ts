import { describe, expect, test } from "bun:test";

const port = process.env.GDA_LIVE_PORT ?? "";
const enabled = port !== "";

const endpoint = `http://127.0.0.1:${port}/mcp`;
const headers = {
  "content-type": "application/json",
  accept: "application/json, text/event-stream",
  "MCP-Protocol-Version": "2026-07-28",
};

const SKILL_NAMES = [
  "godot-autopilot",
  "godot-autopilot-content",
  "godot-autopilot-csharp",
  "godot-autopilot-resources",
  "godot-autopilot-runtime",
  "godot-autopilot-scene-system",
  "godot-autopilot-scripting",
  "godot-autopilot-servers",
  "godot-autopilot-setup",
  "godot-autopilot-tools",
];

async function request(method: string, params: Record<string, unknown>): Promise<Record<string, unknown>> {
  const response = await fetch(endpoint, {
    method: "POST",
    headers,
    body: JSON.stringify({ jsonrpc: "2.0", id: 1, method, params }),
  });
  const body = (await response.json()) as { result?: Record<string, unknown>; error?: { message?: string } };
  if (body.error !== undefined) throw new Error(`${method}: ${body.error.message ?? "request failed"}`);
  if (body.result === undefined) throw new Error(`${method}: missing result`);
  return body.result;
}

describe.skipIf(!enabled)("live modern skills", () => {
  test("server/discover advertises 2026-07-28 and skills extension", async () => {
    const result = await request("server/discover", {});
    expect(result.supportedVersions).toContain("2026-07-28");
    const capabilities = result.capabilities as { extensions?: Record<string, unknown> } | undefined;
    expect(capabilities?.extensions?.["io.modelcontextprotocol/skills"]).toEqual({});
  });

  test("skills/list returns the 10 bundled books", async () => {
    await request("server/discover", {});
    const result = await request("skills/list", { _meta: {} });
    expect(result.resultType).toBe("complete");
    const skills = result.skills as Array<{ frontmatter?: { name?: string }; uri?: string }>;
    expect(skills.map((skill) => skill.frontmatter?.name)).toEqual(SKILL_NAMES);
    for (const skill of skills) {
      expect(skill.uri).toBe(`godot://skills/${skill.frontmatter?.name}/SKILL.md`);
    }
  });

  test("skills/get and resources/read serve the same fallback content", async () => {
    await request("server/discover", {});
    const uri = "godot://skills/godot-autopilot/SKILL.md";
    const skill = await request("skills/get", { uri, _meta: {} });
    const resource = await request("resources/read", { uri, _meta: {} });
    const skillBody = (skill.skill as { content?: string; frontmatter?: { name?: string } } | undefined);
    expect(skillBody?.frontmatter?.name).toBe("godot-autopilot");
    const contents = resource.contents as Array<{ text?: string; mimeType?: string }>;
    expect(contents[0]?.mimeType).toBe("text/markdown");
    expect(contents[0]?.text).toContain("name: godot-autopilot");
    expect(skill.resultType).toBe("complete");
  });
});
