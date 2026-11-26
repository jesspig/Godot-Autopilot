import type { GlobalOptions } from "../args";

import {
  type DomainCommand,
  flagBool,
  flagNumber,
  flagString,
  parseJsonValue,
  readArgsPayload,
  runDomainGroup,
} from "./scene";

function timeoutMs(opts: GlobalOptions): number | undefined {
  const seconds = flagString(opts, "--timeout");
  if (seconds === undefined) return undefined;
  const parsed = Number(seconds);
  return Number.isFinite(parsed) ? parsed * 1000 : undefined;
}

const GAME_COMMANDS: Readonly<Record<string, DomainCommand>> = {
  start: {
    tool: "editor_scene_control",
    risk: "high",
    action: "play",
    build: (opts) => {
      const args: Record<string, unknown> = { action: "play" };
      const path = flagString(opts, "--path");
      if (path !== undefined) args.path = path;
      return args;
    },
  },
  play: {
    tool: "editor_scene_control",
    risk: "high",
    action: "play",
    build: (opts) => {
      const args: Record<string, unknown> = { action: "play" };
      const path = flagString(opts, "--path");
      if (path !== undefined) args.path = path;
      return args;
    },
  },
  stop: {
    tool: "editor_scene_control",
    risk: "high",
    action: "stop",
    build: () => ({ action: "stop" }),
  },
  status: {
    tool: "get_game_state",
    risk: "none",
    action: "status",
    build: (opts) => {
      const args: Record<string, unknown> = { action: "status" };
      const budget = timeoutMs(opts);
      if (budget !== undefined) args.timeout_ms = budget;
      return args;
    },
  },
  input: {
    tool: "game_input",
    risk: "high",
    action: "queue",
    build: (opts) => {
      const args: Record<string, unknown> = { ...readArgsPayload(opts), action: "queue" };
      const type = flagString(opts, "--type");
      const mode = flagString(opts, "--mode");
      const event = flagString(opts, "--event");
      const duration = flagNumber(opts, "--duration");
      if (type !== undefined) args.type = type;
      if (mode !== undefined) args.mode = mode;
      if (event !== undefined) args.event = parseJsonValue(event, "--event");
      if (duration !== undefined) args.duration_ms = duration;
      return args;
    },
  },
  capture: {
    tool: "capture_editor_viewport",
    risk: "low",
    build: (opts) => {
      const args: Record<string, unknown> = {};
      const target = flagString(opts, "--target");
      const dir = flagString(opts, "--dir");
      const when = flagString(opts, "--when");
      const afterFrames = flagNumber(opts, "--after-frames");
      const scale = flagNumber(opts, "--scale");
      if (target !== undefined) args.target = target;
      if (dir !== undefined) args.save_dir = dir;
      if (when !== undefined) args.when = when;
      if (afterFrames !== undefined) args.after_frames = afterFrames;
      if (scale !== undefined) args.scale = scale;
      if (flagBool(opts, "--save")) args.save = true;
      const budget = timeoutMs(opts);
      if (budget !== undefined) args.timeout_ms = budget;
      return args;
    },
  },
  eval: {
    tool: "game_script",
    risk: "high",
    action: "execute",
    build: (opts) => {
      const args: Record<string, unknown> = { ...readArgsPayload(opts), action: "execute", script_action: "script" };
      const source = flagString(opts, "--source");
      if (source !== undefined) args.source_code = source;
      const budget = timeoutMs(opts);
      if (budget !== undefined) args.timeout_ms = budget;
      return args;
    },
  },
  scenario: {
    tool: "game_input",
    risk: "high",
    action: "sequence",
    build: (opts) => {
      const args: Record<string, unknown> = { ...readArgsPayload(opts), action: "sequence" };
      const budget = timeoutMs(opts);
      if (budget !== undefined) args.timeout_ms = budget;
      return args;
    },
  },
};

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  return runDomainGroup("game", GAME_COMMANDS, positional, opts);
}
