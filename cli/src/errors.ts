export const ExitCode = {
  Success: 0,
  ToolFailure: 1,
  Transport: 2,
  Usage: 3,
} as const;

export class CliError extends Error {
  constructor(
    public code: number,
    message: string,
    public suggestion?: string,
  ) {
    super(message);
    this.name = "CliError";
  }
}

export function markIpcRequestSent(error: CliError): CliError {
  (error as CliError & { ipcRequestSent?: boolean }).ipcRequestSent = true;
  return error;
}

export function isIpcRequestSent(error: unknown): boolean {
  return error instanceof CliError &&
    (error as CliError & { ipcRequestSent?: boolean }).ipcRequestSent === true;
}

export function markIpcAuthRejected(error: CliError): CliError {
  (error as CliError & { ipcAuthRejected?: boolean }).ipcAuthRejected = true;
  return error;
}

export function isIpcAuthRejected(error: unknown): boolean {
  return error instanceof CliError &&
    (error as CliError & { ipcAuthRejected?: boolean }).ipcAuthRejected === true;
}

export function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}
