# Setup

## 前置检查

```bash
gda setup status
gda doctor run --offline --ci
```

Observe the setup state and the doctor report: it lists missing CLI, plugin or
runtime pieces with remediation codes.

## 操作：项目初始化

```bash
gda setup init
gda editor start --project <dir>
gda status --format json
```

`setup init` adds the plugin and runtime to the current project. Add the C#
runtime with `--with-csharp-runtime`. `editor start` opens an editor for the
project directory; `status` confirms a live instance and its connection record.

## 操作：技能与插件

```bash
gda skill list --zh
gda skill show <name> --skill-file <rel>
gda skill install --path <dir> --prune
gda setup install --artifact skills --zh
gda plugin install --with-csharp-runtime
```

Skills are embedded in the CLI, so list and show work offline. Skill install
writes the books to an agent skills directory; `--path` overrides the target and
`--prune` removes retired books. Setup install with the skills artifact installs
the archive, and `--zh` selects the Chinese one. Plugin install needs explicit
acknowledgement before managed files are replaced.

## 观察

```bash
gda editor list
gda setup status
```

Check that the instance is live, the project path matches, and the installed
skill or plugin files reflect what you just wrote.

## 验证

```bash
gda ping
gda doctor run --ci
```

## 失败分支

- No instance visible: start an editor, then re-read status. Never choose a
  listen port.
- Project pin and Godot binary disagree: pass the matching `--godot` path.
- Install refuses to replace managed files: review the target, then re-run with
  the force-install option reported by the command.
