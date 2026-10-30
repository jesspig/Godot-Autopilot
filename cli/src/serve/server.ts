import type { EndpointKind, PluginInstanceRecord } from "../instances";
import { readPluginInstanceIndex } from "../instances";
import { CLI_VERSION } from "../version";
import { checkBearer } from "./auth";
import {
  activeRecords,
  checkInstanceReachable,
  controlInstancesPayload,
  instanceEndpoint,
  instanceLabel,
  instanceSummary,
  matchRoute,
  resolveInstanceByRef,
  selectDefaultInstance,
  upstreamUrl,
  type ControlRoute,
} from "./router";

export interface ControlServerOptions {
  hostname: string;
  port: number;
  authToken: string | null;
  timeoutSec: number;
}

interface HandledResponse {
  label: string;
  response: Response;
}

const DROPPED_REQUEST_HEADERS = new Set([
  "authorization",
  "host",
  "connection",
  "keep-alive",
  "proxy-authenticate",
  "proxy-authorization",
  "te",
  "trailer",
  "transfer-encoding",
  "upgrade",
]);

const DROPPED_RESPONSE_HEADERS = ["content-length", "content-encoding", "transfer-encoding"];

export function startControlServer(options: ControlServerOptions) {
  const startedAt = Date.now();
  return Bun.serve({
    hostname: options.hostname,
    port: options.port,
    fetch: async (req) => {
      const requestStartedAt = Date.now();
      const url = new URL(req.url);
      const route = matchRoute(url.pathname);
      let handled: HandledResponse;
      try {
        handled = await handleRequest(req, route, options, startedAt);
      } catch (error) {
        handled = {
          label: "-",
          response: jsonResponse(500, {
            error: "internal_error",
            message: error instanceof Error ? error.message : String(error),
          }),
        };
      }
      process.stdout.write(
        `${new Date().toISOString()} ${req.method} ${url.pathname} -> ${handled.label} ${handled.response.status} ${Date.now() - requestStartedAt}ms\n`,
      );
      return handled.response;
    },
  });
}

async function handleRequest(
  req: Request,
  route: ControlRoute,
  options: ControlServerOptions,
  startedAt: number,
): Promise<HandledResponse> {
  if (route.kind === "health") {
    return {
      label: "control",
      response: jsonResponse(200, {
        ok: true,
        service: "gda-serve",
        version: CLI_VERSION,
        uptimeSec: Math.floor((Date.now() - startedAt) / 1000),
      }),
    };
  }

  if (options.authToken !== null && !checkBearer(req.headers.get("authorization"), options.authToken)) {
    return {
      label: "-",
      response: jsonResponse(
        401,
        { error: "unauthorized", message: "send Authorization: Bearer <control token>" },
        { "WWW-Authenticate": "Bearer" },
      ),
    };
  }

  if (route.kind === "not_found") {
    return {
      label: "-",
      response: jsonResponse(404, { error: "not_found", message: `no control route for ${req.method}` }),
    };
  }

  const records = readPluginInstanceIndex();

  if (route.kind === "instances") {
    return { label: "control", response: jsonResponse(200, controlInstancesPayload(records)) };
  }

  if (route.kind === "instance") {
    const record = resolveInstanceByRef(records, route.ref);
    if (record === undefined) {
      return {
        label: "-",
        response: jsonResponse(404, {
          error: "instance_not_found",
          message: `no instance matches ${route.ref}`,
        }),
      };
    }
    return await forwardTo(req, record, route.endpoint, route.restPath, options);
  }

  const selection = selectDefaultInstance(records);
  if (selection.kind === "none") {
    return {
      label: "-",
      response: jsonResponse(503, { error: "no_active_instance", message: "no active Godot Autopilot instance" }),
    };
  }
  if (selection.kind === "ambiguous") {
    return {
      label: "-",
      response: jsonResponse(409, {
        error: "instance_ambiguous",
        message: "more than one active instance; address one through /i/<id> or /i/<port>",
        candidates: selection.candidates,
      }),
    };
  }
  return await forwardTo(req, selection.record, route.endpoint, route.restPath, options);
}

async function forwardTo(
  req: Request,
  record: PluginInstanceRecord,
  kind: EndpointKind,
  restPath: string,
  options: ControlServerOptions,
): Promise<HandledResponse> {
  const label = instanceLabel(record);
  const endpoint = instanceEndpoint(record, kind);
  const reachable = endpoint !== null && (await checkInstanceReachable(record, kind));
  if (!reachable || endpoint === null) {
    return {
      label,
      response: jsonResponse(503, {
        error: "instance_unavailable",
        message: `instance ${label} is not reachable`,
        instance: instanceSummary(record),
      }),
    };
  }

  const headers = new Headers();
  req.headers.forEach((value, name) => {
    if (DROPPED_REQUEST_HEADERS.has(name.toLowerCase())) return;
    headers.set(name, value);
  });
  if (record.apiToken !== "") headers.set("Authorization", `Bearer ${record.apiToken}`);

  let upstream: Response;
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), options.timeoutSec * 1000);
  const search = new URL(req.url).search;
  try {
    upstream = await fetch(upstreamUrl(endpoint, `${restPath}${search}`), {
      method: req.method,
      headers,
      body: req.body,
      redirect: "manual",
      signal: controller.signal,
    });
  } catch (error) {
    if (isAbortError(error)) {
      return {
        label,
        response: jsonResponse(504, {
          error: "upstream_timeout",
          message: `instance ${label} did not answer within ${options.timeoutSec}s`,
        }),
      };
    }
    return {
      label,
      response: jsonResponse(502, {
        error: "upstream_unreachable",
        message: `cannot reach instance ${label}: ${error instanceof Error ? error.message : String(error)}`,
      }),
    };
  } finally {
    clearTimeout(timer);
  }

  const responseHeaders = new Headers(upstream.headers);
  for (const name of DROPPED_RESPONSE_HEADERS) responseHeaders.delete(name);
  responseHeaders.set("X-GDA-Instance", label);
  return {
    label,
    response: new Response(upstream.body, {
      status: upstream.status,
      statusText: upstream.statusText,
      headers: responseHeaders,
    }),
  };
}

function isAbortError(error: unknown): boolean {
  if (typeof error !== "object" || error === null) return false;
  const name = (error as { name?: unknown }).name;
  return name === "AbortError" || name === "TimeoutError";
}

function jsonResponse(
  status: number,
  payload: unknown,
  extraHeaders: Record<string, string> = {},
): Response {
  return new Response(JSON.stringify(payload), {
    status,
    headers: { "Content-Type": "application/json", ...extraHeaders },
  });
}

export function countActiveInstances(): number {
  return activeRecords(readPluginInstanceIndex()).length;
}
