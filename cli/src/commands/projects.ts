import type { GlobalOptions } from "../args";
import { ExitCode } from "../errors";
import { err } from "../output";
import { listProjects } from "../projects";
import { writeJson } from "./json-output";

export async function run(_positional: string[], opts: GlobalOptions): Promise<number> {
  const projects = listProjects();
  writeJson(projects, opts);
  if (projects.length === 0) {
    err("no projects found under the plugin root; open a Godot project with the plugin enabled once, then retry.");
  }
  return ExitCode.Success;
}