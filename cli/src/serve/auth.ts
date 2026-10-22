import crypto from "node:crypto";
import fs from "node:fs";

import { CliError, ExitCode } from "../errors";
import { gdaRoot, joinPath } from "../platform";

export const CONTROL_TOKEN_FILE = "control.token";
export const CONTROL_TOKEN_BYTES = 32;

export interface ControlToken {
  token: string;
  path: string;
  created: boolean;
}

export function controlTokenPath(): string {
  return joinPath(gdaRoot(), CONTROL_TOKEN_FILE);
}

export function ensureControlToken(): ControlToken {
  const root = gdaRoot();
  const tokenPath = joinPath(root, CONTROL_TOKEN_FILE);
  fs.mkdirSync(root, { recursive: true });
  const existing = readExistingToken(tokenPath);
  if (existing !== null) return { token: existing, path: tokenPath, created: false };
  const token = crypto.randomBytes(CONTROL_TOKEN_BYTES).toString("hex");
  try {
    fs.writeFileSync(tokenPath, token, { mode: 0o600 });
    fs.chmodSync(tokenPath, 0o600);
  } catch (error) {
    throw new CliError(
      ExitCode.Transport,
      `cannot write the control token file at ${tokenPath}`,
      (error as Error).message,
    );
  }
  return { token, path: tokenPath, created: true };
}

function readExistingToken(tokenPath: string): string | null {
  let raw: string;
  try {
    raw = fs.readFileSync(tokenPath, "utf8");
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code === "ENOENT") return null;
    throw new CliError(
      ExitCode.Transport,
      `cannot read the control token file at ${tokenPath}`,
      (error as Error).message,
    );
  }
  const token = raw.trim();
  return token === "" ? null : token;
}

export function extractBearer(header: string | null): string | null {
  if (header === null) return null;
  const match = /^Bearer\s+(\S+)\s*$/i.exec(header.trim());
  return match === null ? null : match[1];
}

export function checkBearer(header: string | null, token: string): boolean {
  const presented = extractBearer(header);
  if (presented === null) return false;
  const received = Buffer.from(presented, "utf8");
  const expected = Buffer.from(token, "utf8");
  if (received.length !== expected.length) return false;
  return crypto.timingSafeEqual(received, expected);
}
