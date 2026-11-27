# bootstrap

## 1. Trigger and inputs
Use this recipe when a fresh agent, a new shell, or a new project session needs a verified connection before any edit. Inputs: a project directory (optional), a known instance id or socket (optional), and a search term plus a tool name for discovery (optional).

## 2. Preconditions
The CLI is on PATH and the local control plane is reachable. If you intend to target an instance, you know the project directory that holds its data. No write operation is assumed yet.

## 3. Copyable CLI commands
```bash
gda ping
gda status --format json
gda doctor run --offline
gda tool list
gda tool search scene
gda tool describe get_scene_tree
gda project list
gda editor list --all-projects
gda skill list
gda docs status
```
Pass complex arguments from a file or stdin instead of guessing a tool name:
```json
{
  "action": "list",
  "category": "scene"
}
```

## 4. Fields to observe at each step
For every call read the envelope fields `ok`, `error`, `data`, `meta`, and `warnings`. The health probe establishes reachability; the status call confirms the bound project and instance fields; diagnostics collect failed checks and remediation codes. Tool list, search, and describe give you the canonical tool names, required parameters, declared side effects, and risk annotations you will rely on later. Skill list and docs status show which local books and documentation caches are available offline.

## 5. Verification and exit criteria
Exit when the health probe succeeds, the status call reports the intended project, and every tool you plan to call has been described at least once. Exit codes: zero success, one business failure, two environment failure, three usage error, four confirmation required, five declined confirmation, one hundred thirty interrupted.

## 6. Failure branches
If the health probe fails, run diagnostics and inspect the CLI log tail. If no instance is listed, check the instance list and the project list. If a tool name is not found, repeat discovery with a broader query and read its schema again; never invent a name.

## 7. Safety and confirmation points
Discovery calls are read-only. If a call returns `confirmation_required`, stop and reissue the same call with --confirm only after reading the tool, target, risk, and argument summary. In non-interactive sessions a missing confirmation means stop. Never resend a call whose outcome is unknown; check the job list first. Treat destructive, process, code-execute, or game-runtime signals as a stop-and-review point.

## 8. Explicit non-goals
Do not hardcode tool counts, alias tables, version windows, or migration history. Do not guess a tool name or parameter without describing it. Do not perform writes, installs, or game runtime changes in this recipe.
