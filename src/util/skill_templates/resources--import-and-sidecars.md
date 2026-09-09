# Import Pipeline and Sidecar Files

How the editor imports assets, how .import and .uid sidecar files are produced and maintained, how duplicate UIDs are resolved, and how the file-system scan state machine gates writes. These are engine behaviors behind `reimport_resource_files`, `scan_editor_file_system`, `set_resource_uid`, `rename_resource_file` and `move_resource_file`.

## The import pipeline

`reimport_resource_files` maps onto the engine's reimport call. Its semantics:

- **Main thread.** The whole reimport pass runs on the editor's main thread; the editor temporarily disables VSync and low-processor-usage mode while it runs. The tool returns once the files are queued, and the work happens during editor idle processing - poll `get_editor_file_system_status` until the import is idle instead of assuming completion.
- **No recursion.** Requesting a reimport while one is already executing is rejected by the engine, not queued.
- **`uid://` inputs are accepted.** Each entry is first resolved through the UID cache: a uid:// string that maps to a known file reimports that file.
- **Import order first.** Files are sorted by each importer's declared import order before anything executes, so low-order importers (on which later importers may depend) run first.
- **Grouped parallelism.** When the editor setting `editor/import/use_multiple_threads` is enabled and an importer supports threaded import, consecutive runs of files using the same importer execute in parallel on the worker thread pool. Single files, non-threaded importers and importer boundaries run sequentially on the main thread.
- **Group files come last.** Imports that bundle several source files (group files) are pulled out of the main pass: their members are skipped and the group file itself is reimported after all regular files.
- **`keep`/`skip` importers are no-ops.** Files whose .import names the `keep` or `skip` pseudo-importer are not imported at all; the engine only refreshes the cached times and MD5 for the file and marks its import state invalid so it is not treated as imported output.
- **Failure writes `valid=false`.** When an import fails, the .import file is still (re)written, but instead of a `path=` output entry it carries `valid=false`. The file then looks import-configured but produces no importable resource - check the .import content when a texture/audio asset refuses to load.

## Writing .import files by hand

Prefer `reimport_resource_files`; hand-write a .import only to repair one that is broken or missing, when you know the importer and its parameters.

- **`[remap]` must be the first section.** The engine writes the file manually in a fixed order and states it directly: order matters, `[remap]` has to go first. Its quick readers rely on that layout.
- **Section layout, in order:**
  - `[remap]` - `importer=`, optional `importer_version=`, optional `type=`, `uid=` (in uid:// text form), optional `group_file=`; then exactly one output declaration: `path=`, one or more `path.<variant>=` lines for variant imports, or `valid=false` when the last import failed; optional `metadata=` and `generator_parameters=` also live here.
  - `[deps]` - `source_file=` and, when there is output, `dest_files=` and `files=`.
  - `[params]` - one line per importer option (`name=value`), written in the importer's declared option order.
- **MD5s live in a separate file.** Checksums are not stored in the .import file: the engine writes a sibling `<import-output>.md5` file (under `.godot/imported/`) holding `source_md5=` and `dest_md5=` lines. This separation is deliberate so .import files stay stable under version control.
- After any hand edit, run `reimport_resource_files` on the file (or `scan_editor_file_system`) so the engine re-reads it and rewrites it in canonical form.

## Importer parameter merge order

For one file the engine assembles the parameter map in this sequence (a later step only fills keys that are still missing, except where noted):

1. **Existing .import `[params]` values** are loaded first.
2. **Importer option defaults** fill every option the file did not set.
3. **`importer_defaults/<importer_name>` from project settings** are applied on top - but only when the importer was matched by file extension, i.e. the .import named no importer or an unknown one. A file whose .import names a valid importer never consults importer_defaults.

Practical consequence: project-wide importer_defaults act as fallbacks for newly discovered files, not as a live override for files that already carry a valid importer name. To change parameters of an already-imported asset, edit its .import `[params]` and reimport.

## Sidecar files

### .uid sidecars

During a scan the engine creates a `<file>.uid` sidecar automatically when all of these hold:

- no `<file>.uid` exists for the path yet,
- a registered loader recognizes the path, and
- that loader cannot embed a UID inside the file itself.

The sidecar is a one-line text file containing the freshly minted UID in uid:// text form. Formats that embed their UID in the file body (text scenes and resources carry it in their header) never get a sidecar; imported assets keep their UID in the .import `[remap]` section instead. Losing a .uid sidecar therefore means the file loses its stable identity: the next scan mints a new UID and every `uid://` reference to the old one breaks.

### Sidecars follow rename/move

The engine's rename/move transaction renames the `.import` and `.uid` sidecars together with the main file, which is what keeps UIDs and import settings stable across moves. A failed sidecar rename does not roll the transaction back: the main file has already moved, and the failure is recorded as an I/O error, which the tools surface as `import_sidecar_warning` (or `import_sidecar_warnings`) in the rename/move report. Check that field rather than assuming both sidecars made it.

## Duplicate UIDs

When a scan finds a file whose UID already maps to a different existing file, nothing fails: the engine silently mints a fresh UID for the newcomer, rewrites it into that file's data, and emits only a WARN (the file keeps working; its identity just changed). The original file keeps its UID, and every existing uid:// reference keeps resolving to the original path - the duplicate never wins the UID. If a duplicated scene or resource "does not react to edits", compare `get_resource_uid` of both files; fix deliberately with `set_resource_uid`.

## Scan state machine and gating

The editor file system runs as a state machine with these observable flags:

- `scanning` - a full scan is in progress.
- `scanning_changes` - an incremental change pass is in progress.
- `importing` - a reimport pass is executing.
- a first-scan marker for the current editor session.

Behaviors that matter for automation:

- **Silent no-op scans.** A scan request issued while any scan/change pass or the scan thread is already running returns immediately without doing anything - no error, no queueing. This is why the autopilot tools fail soft with a retryable readiness error instead of issuing the scan.
- **Change notifications queue.** File changes detected while the system is busy are held in a pending queue and processed only once the system is no longer scanning; they are not lost, but they are not visible until the pending pass runs.
- **First scan is main-thread.** The first scan of an editor session must run on the main thread because it registers global classes, plugins and autoloads; subsequent scans run on a low-priority background thread.
- **Deferred notification.** The filesystem_changed notification raised from a background thread is deferred onto the main loop, so UI and tools observe it one step after the underlying change.
- **The only reliable gates** are the engine's is_scanning, is_importing and doing_first_scan states - signals and timings are advisory. `get_editor_file_system_status` exposes them; while busy, resource write operations return the retryable soft error documented in the main skill.

Reliable write workflow: write or reimport - poll `get_editor_file_system_status` until idle - only then verify with `get_resource_dir_files`, `get_resource_type` or `find_in_files`.

## See also

- references/tool-reference.md - the resource and text-file tool tables
- the main SKILL.md of this skill - load/save, transactional rename/move, deletion and path rules
- godot-autopilot-scene-system - uid:// tokens and ext_resource entries inside .tscn files
- godot-autopilot - the discovery protocol, the error watermark and retry semantics
