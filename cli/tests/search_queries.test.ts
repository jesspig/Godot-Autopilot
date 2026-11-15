import fs from "node:fs";
import path from "node:path";

import { describe, expect, test } from "bun:test";

type SearchQuery = {
  id: string;
  query: string;
  expect_any: string[];
};

type SearchQueryFile = {
  schema: number;
  queries: SearchQuery[];
};

const dataPath = path.resolve(import.meta.dir, "../../tests/data/search_queries.json");
const data = JSON.parse(fs.readFileSync(dataPath, "utf8")) as SearchQueryFile;

describe("search queries baseline", () => {
  test("contains fifty unique well-formed queries", () => {
    expect(data.schema).toBe(1);
    expect(data.queries.length).toBe(50);
    const ids = new Set<string>();
    for (const item of data.queries) {
      expect(item.id.trim().length).toBeGreaterThan(0);
      expect(ids.has(item.id)).toBe(false);
      ids.add(item.id);
      expect(item.query.trim().length).toBeGreaterThan(0);
      expect(item.expect_any.length).toBeGreaterThan(0);
      for (const name of item.expect_any) {
        expect(name.trim().length).toBeGreaterThan(0);
      }
    }
  });
});
