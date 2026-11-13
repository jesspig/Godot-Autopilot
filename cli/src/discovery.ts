export const MCP_PROTOCOL_TOOLS: ReadonlySet<string> = new Set([
  "ping",
  "search_tools",
  "list_categories",
  "get_tool_detail",
  "call_tool",
]);

export function isProtocolTool(name: string): boolean {
  return MCP_PROTOCOL_TOOLS.has(name);
}

export function filterProtocolToolNames(names: string[]): string[] {
  return names.filter((name) => !isProtocolTool(name));
}

export function filterProtocolTools<T extends { name: string }>(entries: T[]): T[] {
  return entries.filter((entry) => !isProtocolTool(entry.name));
}
