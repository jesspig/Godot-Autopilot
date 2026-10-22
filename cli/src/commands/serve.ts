import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { err } from "../output";
import { parseTargetPort } from "../target";
import { ensureControlToken, type ControlToken } from "../serve/auth";
import { countActiveInstances, startControlServer } from "../serve/server";

const DEFAULT_CONTROL_PORT = 9700;
const DEFAULT_TIMEOUT_SEC = 60;
const LOOPBACK_HOSTS = new Set(["127.0.0.1", "::1", "localhost"]);

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  try {
    return await serve(positional, opts);
  } catch (error) {
    if (error instanceof CliError) {
      err(error.message);
      if (error.suggestion !== undefined) {
        process.stderr.write(`[gda] suggestion: ${error.suggestion}\n`);
      }
      return error.code;
    }
    throw error;
  }
}

async function serve(positional: string[], opts: GlobalOptions): Promise<number> {
  if (positional.length > 0) {
    throw new CliError(
      ExitCode.Usage,
      "usage: gda serve [--port <port>] [--bind <addr>] [--no-auth] [--timeout <sec>]",
    );
  }

  const port = opts.portText !== undefined ? parseTargetPort(opts.portText) : DEFAULT_CONTROL_PORT;
  const hostname = parseBind(opts.bind);
  const timeoutSec = parseTimeoutSec(opts.timeoutText);
  const controlToken = opts.noAuth === true ? null : ensureControlToken();

  let server: ReturnType<typeof startControlServer>;
  try {
    server = startControlServer({
      hostname,
      port,
      authToken: controlToken === null ? null : controlToken.token,
      timeoutSec,
    });
  } catch (error) {
    throw new CliError(
      ExitCode.Transport,
      `cannot listen on http://${displayHost(hostname)}:${port}: ${error instanceof Error ? error.message : String(error)}`,
      `port ${port} may already be in use; pass --port <port> to choose another port (the gda serve default is ${DEFAULT_CONTROL_PORT})`,
    );
  }

  process.stdout.write(`gda serve listening on http://${displayHost(hostname)}:${server.port}\n`);
  process.stdout.write(`authentication: ${controlToken === null ? "disabled" : "enabled"}\n`);
  if (controlToken === null) {
    err(
      "WARNING: control-plane authentication is disabled (--no-auth); any local process can call editor tools through this listener",
    );
  } else {
    reportToken(controlToken);
  }
  process.stdout.write(`active instances: ${countActiveInstances()}\n`);

  await waitForShutdown();
  server.stop();
  process.stdout.write("gda serve stopped\n");
  return ExitCode.Success;
}

function reportToken(controlToken: ControlToken): void {
  process.stdout.write(`control token file: ${controlToken.path}${controlToken.created ? " (created)" : ""}\n`);
  process.stdout.write(
    'hint: send the token from that file as "Authorization: Bearer <token>" to /control/instances or /i/<id>/api/...\n',
  );
}

function parseBind(text: string | undefined): string {
  if (text === undefined) return "127.0.0.1";
  const value = text.trim();
  if (LOOPBACK_HOSTS.has(value.toLowerCase())) return value;
  throw new CliError(
    ExitCode.Usage,
    `--bind must be a loopback address (127.0.0.1, ::1, localhost), got: ${text}`,
    "gda serve refuses non-loopback binds; use an SSH tunnel or a reverse proxy for remote access",
  );
}

function displayHost(hostname: string): string {
  return hostname.includes(":") ? `[${hostname}]` : hostname;
}

function parseTimeoutSec(text: string | undefined): number {
  if (text === undefined) return DEFAULT_TIMEOUT_SEC;
  const n = Number(text);
  if (!Number.isFinite(n) || n <= 0) {
    throw new CliError(ExitCode.Usage, `--timeout must be a positive number of seconds, got: ${text}`);
  }
  return n;
}

function waitForShutdown(): Promise<void> {
  return new Promise((resolve) => {
    const done = (): void => {
      process.off("SIGINT", done);
      process.off("SIGTERM", done);
      resolve();
    };
    process.once("SIGINT", done);
    process.once("SIGTERM", done);
  });
}
