# asset-pipeline

## 1. Trigger and inputs
Use this recipe when importing, inspecting, editing, or re-saving project resources and assets. Inputs: a resource path, the property name and new value to set, an asset path to import, and the overwrite target for a copy or save.

## 2. Preconditions
The project is open and the resource or asset path exists. You have described the generic tool you intend to call. You know whether the destination already exists, because overwriting it needs explicit confirmation.

## 3. Copyable CLI commands
```bash
gda resource list
gda resource inspect --path res://data/enemy.tres
gda resource get --path res://data/enemy.tres --property health
gda resource set --path res://data/enemy.tres --property health --value 20
gda resource save --path res://data/enemy.tres --dest res://data/enemy.tres
gda resource import --path res://art/hero.png --args-file -
gda resource references --path res://data/enemy.tres
gda tool call resource_property --args-file - --confirm
```
The argument payload for the generic call:
```json
{
  "action": "save",
  "path": "res://data/enemy.tres",
  "target": "res://data/enemy.tres",
  "overwrite": true,
  "confirm": true
}
```

## 4. Fields to observe at each step
From listing and inspection read the resource type, metadata, and registered name. From the property read confirm the current value and its envelope `ok` and `error`. After a write, read the property again. After saving, confirm the destination path and that no unintended duplicate was created. After import, check for import errors in `warnings`. From reference lookup record every dependent path before renaming or replacing anything.

## 5. Verification and exit criteria
Exit when the property read-back shows the new value, the save reports the intended destination, the import has no errors, and all dependent references still resolve.

## 6. Failure branches
If a property write is rejected, inspect `error` and re-check the property name and type. If the call returns confirmation_required, supply --confirm with the reviewed summary. If the import fails, verify the asset path and retry the import; do not repeatedly resend a call whose result is unknown. If overwrite is refused, inspect the target and confirm it explicitly.

## 7. Safety and confirmation points
Resource and scene overwrites require explicit confirmation. When a high-risk call returns confirmation_required, reissue the same call with --confirm after reviewing tool, target, risk, and arguments. In non-interactive sessions, a missing confirmation means stop. Never resend a call whose outcome is unknown; read the resource back first. Treat destructive, process, code-execute, or game-runtime signals as a stop-and-review point.

## 8. Explicit non-goals
Do not guess a resource type, property name, or tool name. Do not hardcode tool counts or version windows. Do not perform bulk destructive cleanup or game runtime changes in this recipe.
