# Offline

## 前置检查

```bash
gda doctor run --offline --ci
gda docs status
```

The CLI works without network access. The offline doctor run reports only local
checks, and the docs status shows whether the engine documentation cache is
present. No operation requires a remote endpoint.

## 操作：缓存与本地目录

```bash
gda docs sync --offline
gda docs class <name>
gda tool list --offline
gda tool describe <tool> --offline
```

Documentation is exported from the local engine binary into the cache. The
offline tool list and describe use the hash-verified offline catalog and skip
live-instance discovery. Schemas learned this way match the schemas used for
calls.

## 操作：技能与更新检查

```bash
gda skill list --zh --offline
gda update check --offline
```

Bundled skill books are embedded in the CLI and read without network access.
The offline update check reports local versions without contacting a remote.

## 离线报告

```bash
gda monitor report <id>
gda monitor sessions
```

A recorded monitor session can be analyzed locally without a live game run.

## 验证

Compare an offline catalog entry with a live describe result once an editor is
available; a mismatch means the offline snapshot is stale.

## 失败分支

- Offline catalog missing an entry: start an editor and discover the tool live;
  do not guess a name.
- Docs cache empty: export documentation from an installed engine binary.
