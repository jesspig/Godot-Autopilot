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
import { run as serve } from "./serve";
import { run as logs } from "./logs";
import { run as api } from "./api";
import { run as endpoints } from "./endpoints";

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
  serve: { run: serve },
  logs: { run: logs },
  api: { run: api },
  endpoints: { run: endpoints },
};
