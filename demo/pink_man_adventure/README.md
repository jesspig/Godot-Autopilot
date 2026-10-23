# Pink Man 大冒险（Pink Man Adventure）

一个从历史 `Example` 工程还原的 2D 平台跳跃示例工程，作为 godot-autopilot（GDA）的端到端验收场地。工程只包含素材、设计文档与干净工程配置，不包含场景、脚本或存档。

- 设计文档入口：[docs/README.md](docs/README.md)
- 素材：173 张 PNG，来自 Pixel Frog 的 Pixel Adventure 1（CC0 1.0）
- 引擎：Godot 4.7（Forward Plus）

## 使用

```bash
uv run main.py build
uv run main.py deploy --demos pink_man_adventure
```

用 Godot 打开本目录，插件位于 `addons/godot-autopilot/`，MCP 端点默认 `127.0.0.1:9527/mcp`。

## 说明

- 本工程无主场景，打开编辑器后不直接运行游戏。
- 自动化 L2 回归测试床在仓库的 `tests/testbed/`，本工程不参与 ctest。
- 本工程由历史提交 `6c50b99^` 的 `Example/` 还原而来，目录名与工程名已按项目命名。

## License

- 美术素材：CC0 1.0 Universal，Pixel Frog，https://pixelfrog-assets.itch.io/pixel-adventure-1
- 工程配置与文档：MIT，见仓库根 `LICENSE`。
