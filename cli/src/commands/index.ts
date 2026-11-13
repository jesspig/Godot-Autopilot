import type { GlobalOptions } from "../args";
import { run as ping } from "./ping";
import { run as status } from "./status";
import { run as categories } from "./categories";
import { run as search } from "./search";
import { run as describe } from "./describe";
import { run as call } from "./call";
import { run as list } from "./list";
import { run as start } from "./start";
import { run as stop } from "./stop";
import { run as restart } from "./restart";

import { run as logs } from "./logs";
import { run as skills } from "./skills";
import { run as tools } from "./tools";
import { run as resources } from "./resources";
import { run as prompts } from "./prompts";
import { run as projects } from "./projects";
import { run as cache } from "./cache";
import { run as install } from "./install";
import { run as update } from "./update";
import { run as trace } from "./trace";

export interface Command {
  run(positional: string[], opts: GlobalOptions): Promise<number>;
}

export const COMMANDS: Record<string, Command> = {
  ping: { run: ping },
  status: { run: status },
  categories: { run: categories },
  search: { run: search },
  describe: { run: describe },
  call: { run: call },
  list: { run: list },
  start: { run: start },
  stop: { run: stop },
  restart: { run: restart },

  logs: { run: logs },
  skills: { run: skills },
  tools: { run: tools },
  resources: { run: resources },
  prompts: { run: prompts },
  projects: { run: projects },
  cache: { run: cache },
  install: { run: install },
  update: { run: update },
  trace: { run: trace },
};