# scene-script

## 1. Trigger and inputs
Use this recipe when a task requires editing an existing scene, adding a node, or creating and attaching a script. Inputs: a scene path, a node type and name, a parent path, a script path, and the patch payload to apply.

## 2. Preconditions
The editor is running and the target scene is known. You have described any tool you will invoke generically. The script path does not already hold work you intend to keep, or you have confirmed the overwrite.

## 3. Copyable CLI commands
```bash
gda scene list
gda scene open res://scenes/main.tscn
gda scene node --type Sprite2D --name Player --parent . --props '{"visible": true}'
gda scene property --path Player --property position --value '[0, 0]'
gda scene tree --max-depth 4
gda script create res://scripts/player.gd --extends Sprite2D
gda script attach --node Player --script res://scripts/player.gd
gda script patch --args-file -
gda script compile --path res://scripts/player.gd
gda scene save --path res://scenes/main.tscn
gda scene verify --max-depth 4
```
The argument payload for the patch step:
```json
{
  "path": "res://scripts/player.gd",
  "action": "replace",
  "source_code": "extends Sprite2D\nfunc _ready():\n\tpass\n"
}
```

## 4. Fields to observe at each step
After opening the scene, read the tree depth and node paths. After node creation and property writes, read back the edited tree to confirm node name, type, parent, and property value. After script creation and attachment, confirm the script path and the node it is bound to. For patch and compile, read the envelope fields `ok`, `error`, `data`, and `warnings`; fix any compile error before saving. After saving, use scene verification to compare the edited tree with the file on disk.

## 5. Verification and exit criteria
Exit when the patch compiles without errors, the script is attached to the intended node, and scene verification reports no difference between the edited tree and the saved file.

## 6. Failure branches
If the patch is rejected, inspect the error fields and retry with the --dry-run option before writing. If compilation fails, correct the source and compile again. If a node path is wrong, re-read the tree and use the actual path. If saving fails, re-check the target path and whether an explicit overwrite confirmation is required.

## 7. Safety and confirmation points
Scene and script writes are local but must be read back. Overwriting an existing scene or script requires explicit confirmation: if a call returns confirmation_required, reissue it with --confirm after reviewing the summary. In non-interactive sessions, a missing confirmation means stop. Never resend a write whose result is unknown; verify by reading back. Treat destructive, process, code-execute, or game-runtime signals as a stop-and-review point.

## 8. Explicit non-goals
Do not assume a node path, tool name, or script class without reading it first. Do not start the game or run input timelines here. Do not hardcode tool counts or version windows.
