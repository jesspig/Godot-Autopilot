# skills-sync

## 1. Trigger and inputs
Use this recipe when installing, updating, or verifying the bundled skill books and their engine documentation. Inputs: the book name, an auxiliary file path, the install target directory, and the artifact to install.

## 2. Preconditions
You know whether the local skills should come from the embedded archive or a refreshed install. You have permission to write to the chosen install directory. You have recorded the current book list so you can compare after the change.

## 3. Copyable CLI commands
```bash
gda skill list
gda skill show godot-autopilot
gda skill show godot-autopilot --skill-file references/recipes/bootstrap.md
gda setup status
gda setup install --artifact skills
gda skill install --path .agents/skills --prune
gda doctor run --offline
gda docs sync
gda docs search CharacterBody2D
gda docs class Node2D --section Methods
```

## 4. Fields to observe at each step
From the book list record names and locality. From the book show confirm the entry card and that the auxiliary file path resolves. Setup status reports the installed state before you change it. After install or sync, compare the new book list with what you recorded and confirm the target directory. Doctor output must show no new failures. Documentation status, search, and class lookups confirm the cache is usable offline.

## 5. Verification and exit criteria
Exit when the expected book is present, the auxiliary file is readable, the install target contains the books, diagnostics pass, and documentation lookups return results without network access.

## 6. Failure branches
If install returns confirmation_required, reissue with --confirm after reviewing target and risk. If a book is missing after install, re-list and check the setup state. If documentation search returns nothing, refresh the cache and search again. If the install target is wrong, stop and choose the correct directory rather than overwriting files in place.

## 7. Safety and confirmation points
Installing skills or plugins is a high-risk operation. When a call returns confirmation_required, reissue the same call with --confirm after reviewing tool, target, risk, and arguments. Pruning removes directories, so confirm what will be removed before agreeing. In non-interactive sessions, a missing confirmation means stop. Never resend a call whose outcome is unknown. Treat destructive, process, code-execute, or game-runtime signals as a stop-and-review point.

## 8. Explicit non-goals
Do not hardcode tool counts, alias tables, version windows, or migration history in the books. Do not edit the generated skill asset by hand. Do not install unrelated artifacts in this recipe.
