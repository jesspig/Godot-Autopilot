# Setup

## 前置检查

```bash
gda setup status
gda doctor run --offline --ci
```

观察 setup 状态与 doctor 报告：它列出缺失的 CLI、插件或运行时部件并给出修复代码。

## 操作：项目初始化

```bash
gda setup init
gda editor start --project <dir>
gda status --format json
```

`setup init` 把插件与运行时加入当前项目。加 C# 运行时用 `--with-csharp-runtime`。`editor start` 为项目目录打开编辑器；`status` 确认 live 实例及其连接记录。

## 操作：技能与插件

```bash
gda skill list --zh
gda skill show <name> --skill-file <rel>
gda skill install --path <dir> --prune
gda setup install --artifact skills --zh
gda plugin install --with-csharp-runtime
```

技能内嵌在 CLI 中，因此 list 与 show 可离线完成。技能安装把书写入 agent skills 目录；`--path` 覆盖目标，`--prune` 移除退役书。带 skills artifact 的 setup install 安装归档，`--zh` 选择中文归档。插件安装在替换受管文件前需要显式认可。

## 观察

```bash
gda editor list
gda setup status
```

确认实例 live、项目路径匹配，且已安装的技能或插件文件反映刚写入的内容。

## 验证

```bash
gda ping
gda doctor run --ci
```

## 失败分支

- 看不到实例：先启动编辑器，再重新读取状态。禁止自选监听端口。
- 项目 pin 与 Godot 二进制不一致：传入匹配的 `--godot` 路径。
- 安装拒绝替换受管文件：审查目标，然后按命令报告的 force-install 选项重跑。
