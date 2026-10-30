import { CliError, ExitCode } from "../errors";
import { endpointBaseUrl, type EndpointEntry } from "../instances";

export const API_TOOLS_SEGMENT = "/tools";

const MAX_ERROR_TEXT_CHARS = 500;

export interface ApiRequestOptions {
  endpoint: EndpointEntry;
  token?: string;
  timeoutSec: number;
  origin: string;
}

export interface ApiResponse {
  status: number;
  body: unknown;
  text: string;
  contentType: string;
}

export function apiUrl(endpoint: EndpointEntry, suffix: string): string {
  return `${endpointBaseUrl(endpoint)}${suffix}`;
}

export function mapStatusToExitCode(status: number): number {
  if (status >= 200 && status < 300) return ExitCode.Success;
  if (status === 400 || status === 404 || status === 405 || status === 409 || status === 422) return ExitCode.Usage;
  return ExitCode.Transport;
}

export function extractServerError(body: unknown, text: string): string | null {
  const fromBody = extractPayloadError(body);
  if (fromBody !== null) return fromBody;
  if (typeof body === "object" && body !== null && !Array.isArray(body)) {
    const doc = body as Record<string, unknown>;
    for (const key of ["message", "detail", "reason"]) {
      const value = doc[key];
      if (typeof value === "string" && value.trim() !== "") return value;
    }
  }
  const raw = typeof body === "string" ? body : text;
  const trimmed = raw.trim();
  if (trimmed === "") return null;
  return trimmed.length > MAX_ERROR_TEXT_CHARS ? `${trimmed.slice(0, MAX_ERROR_TEXT_CHARS)}...` : trimmed;
}

export function extractPayloadError(body: unknown): string | null {
  if (typeof body !== "object" || body === null || Array.isArray(body)) return null;
  const doc = body as Record<string, unknown>;
  const error = doc["error"];
  if (error !== undefined && error !== null) {
    if (typeof error === "string") return error.trim() === "" ? null : error;
    if (typeof error === "object") {
      const message = (error as Record<string, unknown>)["message"];
      if (typeof message === "string" && message.trim() !== "") return message;
      return JSON.stringify(error);
    }
    return String(error);
  }
  if (doc["ok"] === false) {
    const message = doc["message"];
    return typeof message === "string" && message.trim() !== "" ? message : "the server reported ok=false";
  }
  return null;
}

export function statusAdvice(status: number): string {
  switch (status) {
    case 400:
      return "Check the tool name and the request body; the server rejected the request.";
    case 401:
      return "The instance requires a bearer token; read api_token from the instance registry.";
    case 403:
      return "The instance rejected the bearer token; refresh the instance registry entry.";
    case 404:
      return "The REST route is unknown; verify the endpoint path and the tool name.";
    case 405:
      return "The REST route does not accept this HTTP method.";
    case 409:
      return "The instance is busy with a conflicting request; retry later.";
    case 413:
      return "The request body is too large for the instance.";
    case 429:
      return "The instance is rate limiting requests; retry in a moment.";
    case 500:
    case 502:
    case 503:
      return "The editor may be closed or busy; open it with the plugin enabled and retry.";
    case 504:
      return "Processing exceeded the server time budget; split the work into smaller calls.";
    default:
      return "Verify the port (--port, GODOT_AUTOPILOT_PORT, instance registry) and retry.";
  }
}

export async function apiRequest(
  method: string,
  suffix: string,
  payload: unknown,
  opts: ApiRequestOptions,
): Promise<ApiResponse> {
  const url = apiUrl(opts.endpoint, suffix);
  const headers: Record<string, string> = { Accept: "application/json" };
  if (payload !== undefined) headers["Content-Type"] = "application/json";
  if (opts.token !== undefined && opts.token !== "") headers["Authorization"] = `Bearer ${opts.token}`;

  let res: Response;
  try {
    res = await fetch(url, {
      method,
      headers,
      body: payload === undefined ? undefined : JSON.stringify(payload),
      signal: AbortSignal.timeout(opts.timeoutSec * 1000),
    });
  } catch (err) {
    if (err && ((err as Error).name === "TimeoutError" || (err as Error).name === "AbortError")) {
      throw new CliError(
        ExitCode.Transport,
        `the REST request to ${url} timed out (${opts.origin}).`,
        "Raise --timeout or split the work into smaller calls.",
      );
    }
    const e = err as { cause?: unknown; message?: unknown } | null;
    const cause = e && e.cause ? `: ${String(e.cause)}` : "";
    const detail = e && e.message ? `${String(e.message)}${cause}` : String(err);
    throw new CliError(
      ExitCode.Transport,
      `cannot reach the REST endpoint at ${url} (${opts.origin}).`,
      `Check that the editor runs with the plugin enabled; ${detail}`,
    );
  }

  let text: string;
  try {
    text = await res.text();
  } catch (err) {
    throw new CliError(
      ExitCode.Transport,
      `cannot read the REST response body from ${url} (${opts.origin}).`,
      String((err as Error)?.message ?? err),
    );
  }

  const contentType = res.headers.get("content-type") || "";
  let body: unknown = text;
  if (contentType.includes("json") || text.trim().startsWith("{") || text.trim().startsWith("[")) {
    try {
      body = JSON.parse(text);
    } catch {
      body = text;
    }
  }
  return { status: res.status, body, text, contentType };
}
