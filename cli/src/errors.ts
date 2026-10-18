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
