# Godot-Autopilot

[![ライセンス](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![ビルド](https://github.com/jesspig/Godot-Autopilot/actions/workflows/ci.yml/badge.svg)](https://github.com/jesspig/Godot-Autopilot/actions)
![Godot](https://img.shields.io/badge/Godot-4.7%2B-478CBF?logo=godotengine&logoColor=white)
![プラットフォーム](https://img.shields.io/badge/platform-Windows%7CLinux%7CmacOS-lightgrey)
[![プロトコル](https://img.shields.io/badge/MCP-Streamable_HTTP-blueviolet)](https://modelcontextprotocol.io/)
![ツール数](https://img.shields.io/badge/tools-452%2B-brightgreen)

> **AIエージェントがGodotゲームのビルド、プレイテスト、デバッグをエディターの内部から行います。**

[English](README.md) | [简体中文](README.zh-CN.md) | [繁體中文](README.zh-TW.md) | **日本語** | [한국어](README.ko.md)

Godot-Autopilotは、プラグインとして**Godotエディター内部**に常駐する[MCP](https://modelcontextprotocol.io/)サーバーです。インストールすると、AIコーディングアシスタント（Claude Code、Cursor、Codex、OpenCodeなど）がユーザーの皆様と同じようにプロジェクトを操作できます。シーンのビルド、プロパティの調整、ゲームの実行、ボタンの押下、スクリーンショットの撮影、ログの読み取りを行い、結果を見ながら反復改善します。

ブリッジプロセスも、エラーをチャット欄にコピー＆ペーストする手間も不要です。サーバーはプロジェクトを開くと起動し、閉じると停止します。

## エンドツーエンドデモ

エンドツーエンドテスト中にAIがゲームキャラクターをリアルタイムで操作している様子です：

![デモ 001](assets/demo-001.gif)

## できること

**452以上**のツールでエディターからゲーム実行までの流れ全体をカバーしています（正確な数はバージョンごとに増えますので、`search_tools`でエージェントに確認してもらいます）。主な機能は次のとおりです：

- **シーンのビルド** — Nodeの作成、名前変更、移動、削除を行い、プロパティの読み書きやシグナルの接続、GridMapによる3Dブロックレベルの構築ができます（例：`create_scene_node`、`property_set`、`signal_connect`、`scene_tree_items`；GridMap：`create_gridmap`、`set_gridmap_cell`、`fill_gridmap_box`）。
- **見た目・音・質感** — メッシュとマテリアル、ライトと環境、オーディオ再生、アニメーションとトゥイーン、UIテーマ、タイルマップを扱えます（例：`create_animation`、`tween_editor_property`、`create_game_tween`、`play_audio_player`、`fill_tilemap_rect`）。
- **物理とナビゲーション** — レイ／形状キャスト、剛体、ナビゲーションマップと経路検索に対応します（例：`intersect_physics_3d_ray`、`create_nav_3d_map`、`get_nav_3d_map_path`）。
- **手：入力とUI自動化** — キー、マウス、ゲームパッドをシミュレートし、名前でエディターのコントロールをクリックしたり、エディターのショートカットを実行したり、実行中のゲームのボタンをクリックしたりできます（例：`click_input_mouse`、`get_editor_ui_elements`、`click_editor_element`、`run_editor_shortcut`、`click_game_ui_element`）。
- **スクリプトとデバッグ** — エディター内やゲーム内でGDScriptを実行し、長時間のゲームスクリプトをバックグラウンドジョブとして起動し、プラグイン／ゲーム／スクリプトエラーのログを読み取ります（例：`execute_script`、`execute_game_script`、`start_game_job`、`get_plugin_log`）。
- **プロジェクトと知識** — プロジェクトのファイルやディレクトリーの読み取り・検索・コピー、プロジェクト設定の確認、エンジン内蔵APIドキュメントのオフライン検索ができます（例：`read_file`、`find_in_files`、`list_directory`、`copy_file`、`get_project_settings`、`get_docs_class`）。
- **プレイと目視確認** — 現在のシーンを起動し、時間指定の入力シーケンスを投入し、エディターとゲームのスクリーンショットを撮影して変更前後を比較します（例：`play_editor_current_scene`、`queue_game_input`、`capture_editor_viewport`、`capture_game_viewport`、`review_scene_visually`）。

## エージェントの使い方

ツール名を覚える必要はありません。エージェントは次のシンプルな流れに従います：

1. **探索** — `search_tools`で候補を探し、`get_tool_detail`でパラメーターを確認します（名前を推測しません）。
2. **実行** — 単発の手順は`call_tool`、一括処理は`batch_execute`で実行します。
3. **検証** — 結果をスクリーンショットで撮影し、前回の撮影結果と比較して修正し、繰り返します。

エージェントが最初から使いこなせるように、プラグインには**自己完結したスキルブック（10冊）**（概要編、さらにシーン編・リソース編・スクリプト編・ランタイム編・サーバー編・コンテンツ編・C#編・セットアップ編・ツール編）が付属します。インストールは`gda install skills`で行います（中国語は`--zh`を追加）。以降、エージェントは推測せずに呼び出しプロトコルに従って操作します。位置づけとして、MCPが主要な製品チャネルであり、スキルは体験層です。呼び出しガイドは、MCPが停止した場合はGDA CLIに切り替わります。

プラグインには1つのエディターパネルが付属します：

- **Log** — プラグイン専用ログをレベル／カテゴリーフィルターと検索付きで表示しますので、異常な呼び出しを追跡しやすくなります。

## はじめに

**Godot 4.7+**が必要です。

1. **プラグインの入手** — プロジェクトのGitHub Releasesページ（[jesspig/Godot-Autopilot](https://github.com/jesspig/Godot-Autopilot)）から`addons.zip`をダウンロードし、`godot-autopilot/`フォルダーをプロジェクトの`addons/`ディレクトリーにコピーします：
    ```
    your-project/
    └── addons/
        └── godot-autopilot/
            ├── godot-autopilot.dll      (Windows)
            ├── libgodot-autopilot.so    (Linux)
            ├── libgodot-autopilot.dylib (macOS)
            └── godot-autopilot.gdextension
    ```
2. **プロジェクトを開きます** — サーバーが自動的に起動し、初回起動時に空きループバックポートを1つバインドします（プロジェクトごとに永続化され、`20000–29999`から選択）。有効化スイッチは不要です。
3. **AIクライアントを接続します** — `gda status --json`を実行し、その`mcp_url`をクライアントのMCP設定に追加します（URLをエージェントに渡して設定を生成させることもできます）。Claude Code、Codex、Cursor、OpenCode、Copilotを含む20のクライアントに対応しています。
4. **高リスクな呼び出しを確認します（任意）** — `destructive`、`process`、`code_execute`、`game_runtime`の副作用を持つツールは`confirm=true`が必要です。グローバルな許可リストはありません。
5. **お願いしてみます** — 例：「走ってジャンプできるプレイヤーキャラクターを作って、プレイテストして変なところを直して」。

ソースからビルドしたり貢献したりしたい場合は、[docs/wiki/](docs/wiki/)（まずは[index.md](docs/wiki/index.md)、ビルドは[build.md](docs/wiki/build.md)）と[tests/README.md](tests/README.md)をご覧ください。

## コマンドラインからの利用

チャットクライアントではなくターミナルから操作したいですか？GDA CLI は実行中の GDA インスタンスをシェルから操作します。MCPクライアントの統合は不要です。Bun + TypeScript 製で、ソースから直接実行でき、**実行時依存関係ゼロ**、ビルド手順もありません。

`bunx` でその場実行します（PowerShell では `#` がコメント開始になるため引用符が必要です）：

```
bunx "github:jesspig/Godot-Autopilot#0.3.3" ping
```

または一度グローバルインストールして、`gda` コマンドを使います：

```
bun install -g github:jesspig/Godot-Autopilot#0.3.3
gda ping
```

コマンドファミリー：**呼び出し系** — `ping` / `status` / `categories` / `search` / `describe` / `call`；**ライフサイクル** — `list` / `start` / `stop` / `restart` / `logs`；**オフラインとアセット系** — `skills` / `tools` / `projects` / `cache` / `install` / `update` / `trace`。

**Bun** が必要です。Bun も Node も無い環境では、Releases の `gda` 単体バイナリを使ってください（詳細は [docs/wiki/modules/cli.md](docs/wiki/modules/cli.md)）。

## AI にセットアップさせる

上の手順を手作業でなぞる代わりに、同じマシンでシェルを実行できるコーディングエージェントに任せられます。下のブロックをコピーし、プロジェクトの絶対パスを埋めて送ってください：

```text
Set up Godot Autopilot (GDA) on this machine end to end, then prove it works.

Project: <ABSOLUTE PATH TO YOUR GODOT PROJECT> (Godot 4.7+).

1. Install the plugin: download addons.zip from
   https://github.com/jesspig/Godot-Autopilot/releases/latest and extract it so that
   <project>/addons/godot-autopilot/godot-autopilot.gdextension exists.
2. Start the editor: run `godot --editor --path <project>` (ask me for the engine path if it is
   not on PATH) and wait until the `mcp_url` from `gda status --json` answers.
3. Verify the server from a shell. Try in this order, stop at the first that works, and tell me
   which one it was:
   - bunx "github:jesspig/Godot-Autopilot#0.3.3" ping
   - download the gda release asset for this OS and CPU from the same Releases page, verify it
     against sha256sums.txt, then run ./gda ping
4. Connect my AI client: add
   {"mcpServers":{"godot-autopilot":{"url":"<mcp_url from gda status --json>"}}}
   to its MCP configuration (or hand the URL to your agent and ask it to generate the config).
5. Summarize: which channel worked, every command you ran, and anything that failed.

Rules: never hand-write HTTP requests against the port; never expose the GDA port beyond loopback;
leave high-risk tools (code_execute, game_runtime, destructive, process) unconfirmed unless I ask for them.
```

クライアントがスキルに対応している場合（Claude Code、Codex、Cursor など）は、`gda install skills`を実行してください。エージェントが推測せずに完全な呼び出しプロトコルを参照できます。

## 安全上の注意

サーバーはお使いのマシンのループバックアドレス上でのみ待ち受けます（ポートはプラグインがプロジェクトごとに選択し、パスは`/mcp`固定）。ローカル外のアドレスは拒否します。高リスクなツール（`destructive`、`process`、`code_execute`、`game_runtime`）は`confirm=true`が必要です。グローバルな許可リストはありません。詳細は[docs/wiki/security_contract.md](docs/wiki/security_contract.md)をご覧ください。

## アセットの帰属

`demo/`配下の5つのサンプルプロジェクト（2D／3D初心者向けチュートリアル、UIコントロールギャラリー、ビューポート構成、ネットワーク対戦Pong）は公式の[Godotデモプロジェクト](https://github.com/godotengine/godot-demo-projects)が由来です。テスト用にアセットとクリーンなプロジェクト構成のみを残しています。6つ目のプロジェクト `demo/pink_man_adventure` は、かつての `Example/` デモプロジェクトから復元したリポジトリ内サンプルです。173枚のPNGアセットは Pixel Frog の Pixel Adventure 1（CC0 1.0）に由来し、設計ドキュメントは本リポジトリのオリジナルです。著作権とライセンスの詳細は各プロジェクトのREADMEとLICENSEファイルに記載されています。

## ライセンス

MIT
