# Offline

## 前置检查

```bash
gda doctor run --offline --ci
gda docs status
```

CLI 无需网络即可工作。离线 doctor 只报告本地检查，docs status 显示引擎文档缓存是否存在。任何操作都不需要远端端点。

## 操作：缓存与本地目录

```bash
gda docs sync --offline
gda docs class <name>
gda tool list --offline
gda tool describe <tool> --offline
```

文档从本地引擎二进制导出到缓存。离线 tool list 与 describe 使用哈希校验过的离线目录，跳过 live 实例发现。这样学到的 schema 与调用时使用的 schema 一致。

## 操作：技能与更新检查

```bash
gda skill list --zh --offline
gda update check --offline
```

内嵌技能书随 CLI 分发，可离线读取。离线更新检查只报告本地版本，不连接远端。

## 离线报告

```bash
gda monitor report <id>
gda monitor sessions
```

已录制的 monitor 会话可在本地分析，无需 live 游戏运行。

## 验证

编辑器可用后，把离线目录条目与 live describe 结果比对；不一致说明离线快照已陈旧。

## 失败分支

- 离线目录缺条目：启动编辑器并 live 发现该工具；不要猜名字。
- 文档缓存为空：从已安装的引擎二进制导出文档。
