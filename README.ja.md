# Godot Autopilot

[![ライセンス](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![最新リリース](https://img.shields.io/github/v/release/jesspig/Godot-Autopilot)](https://github.com/jesspig/Godot-Autopilot/releases)
[![ビルド](https://github.com/jesspig/Godot-Autopilot/actions/workflows/ci.yml/badge.svg)](https://github.com/jesspig/Godot-Autopilot/actions)
[![リリースワークフロー](https://github.com/jesspig/Godot-Autopilot/actions/workflows/release.yml/badge.svg)](https://github.com/jesspig/Godot-Autopilot/actions/workflows/release.yml)
![Godot](https://img.shields.io/badge/Godot-4.7%2B-478CBF?logo=godotengine&logoColor=white)
![プラットフォーム](https://img.shields.io/badge/platform-Windows%7CLinux%7CmacOS-lightgrey)
[![CLI](https://img.shields.io/badge/CLI-gda-ネイティブ-blue)](docs/wiki/modules/cli.md)
![ツール](https://img.shields.io/badge/tools-252-brightgreen)

> **AIエージェントがGodotゲームのビルド、プレイテスト、デバッグをエディターの内部から行います。**

[English](README.md) | [简体中文](README.zh-CN.md) | [繁體中文](README.zh-TW.md) | **日本語** | [한국어](README.ko.md)

> 本翻訳は英語版 [README.md](README.md) を正とします。

Godot Autopilot (GDA) は、プラグインとして**Godot エディターの内部**に常駐し、単一のローカル `gda` CLI を通じて実行中のプロジェクトを公開します。設定が必要なプロトコルやエンドポイントはありません。AI コーディングアシスタント（Claude Code、Cursor、Codex、OpenCode など）は CLI コマンドでユーザーと同じようにプロジェクトを操作します。シーンの構築、プロパティの調整、ゲームの実行、ボタンの押下、スクリーンショットの撮影、ログの読み取りを行い、その結果を見ながら反復改善します。ブリッジプロセスも、エラーをチャット欄にコピー＆ペーストする手間も不要です。グラフィカルなエディターセッションでは、プロジェクトを開くとインスタンスが自動登録され、閉じると登録が解除されます。

![デモ 001](assets/demo-001.gif)

**252 個のツール**がエディターからゲーム実行までのループ全体をカバーします：246 個のドメインツール、5 個のメタツール、`system_status`（正確な数はバージョンごとに増えます。`gda tool list` で確認してください）。325 個の旧ツール名エイリアスは 0.6.0 まで `gda tool describe` で解決し、`gda tool call` で呼び出せます（実名ツールと重複する場合は実名ツールが優先）。スキルブックは **入口カード 1 枚と references 9 篇**で、CLI の呼び出しプロトコルをエージェントに教えます。参考：[ツール面](docs/wiki/tool_surface_baseline.md)、[スキル](docs/wiki/modules/skills.md)、[CLI](docs/wiki/modules/cli.md)。

## クイックスタート

**Godot 4.7+** とグラフィカルなエディターセッションが必要です。

1. **インストール** — Godot プロジェクトのルートでワンクリックスクリプトを実行するか、手動でのリリースインストールとソースビルドの手順については [INSTALL.md](INSTALL.md) に従ってください。
   - macOS/Linux:

     ```bash
     curl -fsSL https://raw.githubusercontent.com/jesspig/Godot-Autopilot/master/scripts/install.sh \
       | bash -s -- --project "$PWD" --skills
     ```

   - Windows PowerShell:

     ```powershell
     & ([scriptblock]::Create((irm https://raw.githubusercontent.com/jesspig/Godot-Autopilot/master/scripts/install.ps1))) `
       -Project (Get-Location).Path -Skills
     ```

   スクリプトは `gda` CLI をインストールし、`project.godot` を検出してプラグインをインストールし、必要に応じてスキルブックもインストールします。
2. **プロジェクトを開く** — グラフィカルなエディターセッションでは、プラグインがインスタンスを CLI に自動登録し、初回起動時に空いているループバックポートをバインドします（プロジェクトごとに永続化され、`20000–29999` から選択）。ヘッドレス／コマンドラインのエディターセッションではインスタンスは提供されません。
3. **CLI で接続する** — `gda ping` と `gda status --format json` を実行し、JSON の接続記録で稼働中のインスタンスとローカル端点を確認します。クライアント設定は不要です。すべての操作は JSON エンベロープを出力する `gda` コマンドです。Claude Code、Codex、Cursor、OpenCode、Copilot などから CLI を直接操作できます。
4. **何かを作ってもらう** — 例：「走ってジャンプできるプレイヤーキャラクターを作って、プレイテストして、おかしいところを直して」。

## 安全上の注意

CLI はループバック上のインスタンスにのみ接続し（ポートはプラグインがプロジェクトごとに選択し、CLI が自動的に接続先を特定し）、ローカル以外のアドレスを拒否します。高リスクツール（`destructive`、`process`、`code_execute`、`game_runtime`）には `--confirm` が必要です。グローバルな許可リストはありません。詳細：[docs/wiki/security_contract.md](docs/wiki/security_contract.md)。

## ドキュメント

- [INSTALL.md](INSTALL.md) — ワンクリックスクリプト、手動でのリリースインストール、ソースビルド、エージェント向けの手順。
- [docs/wiki/index.md](docs/wiki/index.md) — プロジェクトの知識ベース：アーキテクチャ、モジュール、テスト、規約。
- [docs/wiki/tool_surface_baseline.md](docs/wiki/tool_surface_baseline.md) — ツール面、エイリアス、schema、ケイパビリティ行列のベースライン。
- [docs/wiki/build.md](docs/wiki/build.md) — ビルド、デプロイ、パッケージング。
- [docs/wiki/modules/cli.md](docs/wiki/modules/cli.md) — CLI の完全なリファレンス。
- [docs/wiki/modules/skills.md](docs/wiki/modules/skills.md) — スキルブックと `gda install skills`。
- [tests/README.md](tests/README.md) — テストの構成とコマンド。

## ライセンス

MIT
