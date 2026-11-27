# long-task

## 1. Trigger and inputs
Use this recipe when a command is marked long-running, when you want progress streamed instead of a blocking wait, or when a run must survive a dropped connection. Inputs: the suite or scenario to start, a job id returned by a detached call, and timeout, retry, and cursor values for polling.

## 2. Preconditions
You have recorded the command you intend to detach and its expected result fields. You know the instance and generation binding to watch, or you will discover it from the job list. You have a place to write progress or reports.

## 3. Copyable CLI commands
```bash
gda test run --label smoke --detach
gda job list
gda job status <id>
gda job wait <id> --timeout 600 --progress
gda job logs <id> --cursor 0 --limit 50
gda job cancel <id> --confirm --reason superseded
gda debug repro --path res://scenes/main.tscn --confirm --detach
gda script test --directory tests --timeout 120
gda test status <id>
```

## 4. Fields to observe at each step
The detached call returns a task id immediately and streams progress. From the job list read id, state, and instance or generation binding. Status shows the current stage and terminal state. Wait preserves the terminal result across reconnects; read its preserved state and any progress lines. Logs from a cursor give ordered events. Cancellation reports the actual stage reached, not just a request. Scene repro and test runs report their own run status and result counts.

## 5. Verification and exit criteria
Exit when the job reaches a terminal state, its result matches the expected fields, progress and logs have been captured, and no further polling is needed. A cancelled job exits only after the cancellation stage is confirmed.

## 6. Failure branches
If the connection drops while waiting, re-check status and wait again with retries; do not resubmit the work. If the job id is unknown, list jobs and match by label and generation. If logs are empty, start from the beginning cursor. If a run must be stopped, cancel with --confirm and verify the reported stage; never assume cancellation succeeded until it is read back.

## 7. Safety and confirmation points
Cancelling a job and running a scene repro are high-risk actions. When a call returns confirmation_required, reissue the same call with --confirm after reviewing tool, target, risk, and arguments. In non-interactive sessions, a missing confirmation means stop. Never resend a call whose outcome is unknown; use the job record instead. Treat destructive, process, code-execute, or game-runtime signals as a stop-and-review point.

## 8. Explicit non-goals
Do not hardcode tool counts or version windows. Do not assume a task id format or a generation number. Do not treat a detached call as finished until its terminal state is read.
