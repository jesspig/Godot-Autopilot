export const ExitCode = {
  Success: 0,
  ToolFailure: 1,
  Transport: 2,
  Usage: 3,
  ConfirmationRequired: 4,
  UserDeclined: 5,
  Interrupted: 130,
} as const;

export const ERROR_CODES: Readonly<Record<number, string>> = {
  [ExitCode.Success]: "OK",
  [ExitCode.ToolFailure]: "TOOL_FAILURE",
  [ExitCode.Transport]: "TRANSPORT_FAILURE",
  [ExitCode.Usage]: "USAGE_ERROR",
  [ExitCode.ConfirmationRequired]: "CONFIRMATION_REQUIRED",
  [ExitCode.UserDeclined]: "USER_DECLINED",
  [ExitCode.Interrupted]: "INTERRUPTED",
};

export function errorCodeForExit(exitCode: number): string {
  return ERROR_CODES[exitCode] ?? "TOOL_FAILURE";
}

export class CliError extends Error {
  constructor(
    public code: number,
    message: string,
    public suggestion?: string,
    public details?: Record<string, unknown>,
  ) {
    super(message);
    this.name = "CliError";
  }
}

export class ConfirmationRequiredError extends CliError {
  constructor(
    message: string,
    public readonly confirmation: Record<string, unknown>,
    suggestion?: string,
  ) {
    super(ExitCode.ConfirmationRequired, message, suggestion, { confirmation });
    this.name = "ConfirmationRequiredError";
  }
}

export class UserDeclinedError extends CliError {
  constructor(
    message: string,
    public readonly confirmation: Record<string, unknown>,
  ) {
    super(ExitCode.UserDeclined, message, "the operation was not executed.", { confirmation });
    this.name = "UserDeclinedError";
  }
}

export class InterruptedError extends CliError {
  constructor(message = "interrupted") {
    super(ExitCode.Interrupted, message, undefined, { interrupted: true });
    this.name = "InterruptedError";
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
