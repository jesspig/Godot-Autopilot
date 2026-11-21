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

export function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}
