# error-recovery

## 1. Trigger and inputs
Use this recipe when a call fails, returns a non-zero exit code, reports confirmation_required, or leaves an outcome unknown. Inputs: the failing command and its exit code, the affected instance or project, and any job id or trace identifier you already have.

## 2. Preconditions
You have not resent the failing call. You know whether the CLI failure was a business error, an environment error, a usage error, or a confirmation gate. No destructive cleanup has been started.

## 3. Copyable CLI commands
```bash
gda doctor run --offline
gda doctor diagnose --ci
gda status
gda logs cli --tail 100 --level error
gda logs plugin --limit 50 --level error
gda trace failures --limit 20
gda trace summary
gda job list
gda tool call resource_property --args-file - --confirm
```
The argument payload for the confirmed generic call:
```json
{
  "action": "set",
  "path": "res://data/config.tres",
  "property": "enabled",
  "value": true,
  "confirm": true
}
```

## 4. Fields to observe at each step
Read the envelope `ok`, `error`, and `warnings`, and inside `error` read `code`, `message`, `retryable`, and `request_sent`. A non-retryable error must not be blindly retried. If `request_sent` is unclear, treat the outcome as unknown and inspect state rather than resending. Diagnostics list failed checks with remediation codes. Status, logs, trace failures, and the job list together tell you whether the side effect happened.

## 5. Verification and exit criteria
Exit when the root cause is identified, the remediation has been applied, the same call succeeds once, and state is confirmed by a read-back. Exit codes: zero success, one business failure, two environment failure, three usage error, four confirmation required, five declined confirmation, one hundred thirty interrupted.

## 6. Failure branches
If diagnostics report an environment failure, fix connectivity before retrying. If it is a usage error, correct flags and arguments. If confirmation_required was returned, reissue the same call with --confirm after reading the summary; if the user declined, stop. If the result is unknown, inspect the job list and read the target state; do not resend.

## 7. Safety and confirmation points
Confirmation is a gate, not a blanket approval. When a call returns confirmation_required, review tool, target, risk, and arguments, then reissue with --confirm. In non-interactive sessions, a missing confirmation means stop. Never resend a call whose outcome is unknown. Treat destructive, process, code-execute, or game-runtime signals as a stop-and-review point.

## 8. Explicit non-goals
Do not hardcode tool counts, alias tables, or version windows. Do not clear caches, cancel jobs, or delete files as a reflexive recovery step. Do not guess a replacement tool name without describing it.
