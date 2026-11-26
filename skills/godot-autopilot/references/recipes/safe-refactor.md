# safe-refactor

## 1. Trigger and inputs
Use this recipe when renaming, moving, or replacing a script or resource that other files depend on. Inputs: the current path, the target path, the patch payload for in-place edits, and the test label used to verify behavior.

## 2. Preconditions
The project is open and you have looked up every reference to the item before changing it. A backup or version-controlled copy exists. The destination path does not already contain work you intend to keep, or you have confirmed the overwrite.

## 3. Copyable CLI commands
```bash
gda resource references --path res://scripts/player.gd
gda script patch --dry-run --args-file -
gda resource rename --path res://scripts/old.gd --to res://scripts/new.gd --confirm
gda script compile --path res://scripts/new.gd
gda script reload --path res://scripts/new.gd
gda scene tree --max-depth 4
gda scene verify --max-depth 4
gda test run --label refactor --junit reports/refactor.xml
```
The argument payload for the dry-run patch:
```json
{
  "path": "res://scripts/player.gd",
  "action": "replace",
  "source_code": "extends Sprite2D\n"
}
```

## 4. Fields to observe at each step
From reference lookup record every dependent path and count. The dry run shows the proposed change and compile outcome without writing. After rename, confirm the new path and that references were rewritten. After compiling and reloading, read the envelope fields `ok`, `error`, and `warnings`. Scene tree and verification confirm no broken node bindings. The test run shows pass and fail counts plus the JUnit report path.

## 5. Verification and exit criteria
Exit when the dry run compiles cleanly, the new path resolves, no broken references remain, the scene verifies against disk, and the labeled test suite passes.

## 6. Failure branches
If the dry run fails, correct the payload and repeat before any write. If rename returns confirmation_required, reissue with --confirm after reviewing the target. If compilation fails after rename, restore the backup and reassess. If tests fail, do not proceed to further renames; inspect the failures first. Never resend a rename whose result is unknown; read the path back.

## 7. Safety and confirmation points
Renames and moves rewrite references and require explicit confirmation. When a call returns confirmation_required, reissue the same call with --confirm after reviewing tool, target, risk, and arguments. In non-interactive sessions, a missing confirmation means stop. Never resend a call whose outcome is unknown. Treat destructive, process, code-execute, or game-runtime signals as a stop-and-review point, and authorize high-risk items one by one in a batch.

## 8. Explicit non-goals
Do not hardcode tool counts, alias tables, or version windows. Do not apply a bulk rename without per-item review. Do not perform game runtime changes in this recipe.
