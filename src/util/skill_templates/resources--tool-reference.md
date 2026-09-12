# Resource Tool Reference

The 26 resource domain tools plus the three text-file tools from the OS domain. Domain tools are invoked through `call_tool`; see the godot-autopilot skill for the protocol.

## Resource domain (26 tools)

| Tool | Purpose |
|---|---|
| `load_resource` | Load a resource file into memory with optional `type_hint`; returns a locator for the property tools. |
| `reload_resource` | Force-reload a resource file from disk with a CACHE_MODE_REPLACE load, replacing the editor's cached instance; returns result (reloaded), path and replaced. Use it so later reads observe on-disk changes instead of a stale cached instance. |
| `load_resource_threaded` | Start an asynchronous background load and return immediately; step 1 of the threaded-load chain. |
| `get_resource_load_threaded_status` | Poll a threaded load: invalid_resource, in_progress, failed or loaded. |
| `get_resource_load_threaded` | Blockingly fetch the resource from a completed threaded load; step 3 of the chain. |
| `save_resource` | Save a resource; `path` is required as the load path and default destination. When `dest_path` differs from `path` and the resource was located through `path`, saves a deep copy so the cached source instance is untouched (response adds copy_on_write and source_path). Creates directories recursively, refreshes the file system, read-back verified. |
| `create_resource` | Instantiate a Resource subclass by `type` - a ClassDB engine class or a global script class - in memory; `name` registers it as a memory:// locator. |
| `duplicate_resource` | Duplicate a file-backed resource; `deep` (default false) copies sub-resources; `name` registers the duplicate as a memory:// locator that stays alive across calls. |
| `get_resource_type` | Report the engine class name a file actually holds. |
| `has_resource` | Check loadability through the engine loader (not plain file existence). |
| `get_resource_types` | List every instantiable Resource subclass registered in the engine. |
| `get_resource_extensions` | List recognized file extensions for a resource `type` (all known types when omitted). |
| `get_resource_dir_files` | List a directory's entries through the resource loader. |
| `get_resource_uid` | Read the numeric UID behind a uid:// reference (-1 when absent). |
| `set_resource_uid` | Assign a UID (auto-generated when `uid` is omitted); triggers a reimport. |
| `remove_resource_file` | Two-stage delete: dry-run impact pre-check by default; force=true moves the file to the OS trash. |
| `rename_resource_file` | Transactional single-file rename: rewrites path=/uid= references, migrates sidecars, remaps project.godot, reports updated_files and stale_references. A directory `path` fails fast - use `move_resource_file`. |
| `move_resource_file` | Move a file or directory tree to another res:// directory through the same reference-rewriting transaction pipeline; the directory walk skips .import/.uid sidecars (each file's transaction migrates them) and stale scene tabs of moved files are closed (closed_tabs). |
| `copy_resource_file` | Byte-for-byte copy of a file to `dest_path` (`path` and `dest_path` both required); creates parent directories, overwrites an existing target, triggers a scan. Does not rewrite references and does not copy sidecars; returns result (copied). |
| `create_directory` | Create a res:// directory idempotently, then trigger a file system scan. |
| `get_resource_dependencies` | List the external files a resource references. |
| `has_resource_dependency` | Check whether a resource depends on one specific file. |
| `get_resource_references` | Reverse lookup: which .tscn/.tres files reference a resource (by `path`, `uid` or `class_name`). |
| `reimport_resource_files` | Queue reimport of one or more files; asynchronous, editor only. |
| `set_resource_property` | Set a property on a resource located by `object_id`/`object_id_str`, `name` or `path`. |
| `get_resource_property` | Read a property value from a located resource. |

## Text-file tools (OS domain)

| Tool | Purpose |
|---|---|
| `write_file` | Write or append text to a file; res:// writes are engine-managed (reimport or update_file), user:// and absolute paths are plain I/O. |
| `read_file` | Read a text file (absolute, res:// or user://) and return its full content. |
| `find_in_files` | Recursively search text files for a query string, with per-file occurrence counts and a truncated flag. |

Property locators: `object_id`/`object_id_str` (from `create_resource` or `load_resource`), in-memory `name`, or file `path`.

## See also

- godot-autopilot - the tool discovery protocol and `call_tool` usage rules
- references/import-and-sidecars.md - engine behavior behind `reimport_resource_files`, sidecar files and scan gating
- godot-autopilot-scene-system - the property and signal tools that consume these resource locators
- godot-autopilot-scripting - script resources and the script diagnostics returned by res:// `write_file`
