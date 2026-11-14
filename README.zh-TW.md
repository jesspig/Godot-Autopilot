# Godot Autopilot

[![授權](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![建置狀態](https://github.com/jesspig/Godot-Autopilot/actions/workflows/ci.yml/badge.svg)](https://github.com/jesspig/Godot-Autopilot/actions)
![Godot 版本](https://img.shields.io/badge/Godot-4.7%2B-478CBF?logo=godotengine&logoColor=white)
![支援平台](https://img.shields.io/badge/platform-Windows%7CLinux%7CmacOS-lightgrey)
[![協定](https://img.shields.io/badge/MCP-Streamable_HTTP-blueviolet)](https://modelcontextprotocol.io/)
![工具數](https://img.shields.io/badge/tools-452%2B-brightgreen)

> **讓 AI 代理在編輯器內部建置、試玩與除錯你的 Godot 遊戲。**

[English](README.md) | [简体中文](README.zh-CN.md) | **繁體中文** | [日本語](README.ja.md) | [한국어](README.ko.md)

Godot Autopilot（GDA）是一個以插件形式**常駐於 Godot 編輯器內部**的 [MCP](https://modelcontextprotocol.io/) 伺服器。安裝完成後，你的 AI 程式設計助理（Claude Code、Cursor、Codex、OpenCode 等）就能像你一樣操作專案：建置場景、調整屬性、執行遊戲、按下按鈕、截圖，並讀取日誌——再依據所見結果反覆迭代。

不需要橋接程序，也不必把錯誤訊息複製貼到聊天視窗。在圖形化編輯器工作階段中，伺服器會隨專案開啟而啟動、隨專案關閉而停止。

## 端到端示範

端到端測試中，AI 即時控制遊戲角色：

![Demo 001](assets/demo-001.gif)

## 能做什麼

超過 **452 個工具**，涵蓋從編輯器到遊戲的完整閉環（實際數量隨版本成長——請代理透過 `search_tools` 查詢）。重點如下：

- **建置場景**——建立、重新命名、移動與刪除節點；讀取與寫入屬性；連接訊號；用 GridMap 搭建 3D 方塊關卡（例如 `create_scene_node`、`property_set`、`signal_connect`、`scene_tree_items`；GridMap：`create_gridmap`、`set_gridmap_cell`、`fill_gridmap_box`）。
- **畫面、聲音與手感**——網格與材質、燈光與環境、音訊播放、動畫與補間、UI 主題、圖塊地圖（例如 `create_animation`、`tween_editor_property`、`create_game_tween`、`play_audio_player`、`fill_tilemap_rect`）。
- **物理與導航**——射線/形狀投射、剛體、導航地圖與路徑查詢（例如 `intersect_physics_3d_ray`、`create_nav_3d_map`、`get_nav_3d_map_path`）。
- **動手：輸入與 UI 自動化**——模擬鍵盤、滑鼠與遊戲手把；依名稱點擊編輯器控制項；執行編輯器快速鍵；點擊執行中遊戲裡的按鈕（例如 `click_input_mouse`、`get_editor_ui_elements`、`click_editor_element`、`run_editor_shortcut`、`click_game_ui_element`）。
- **腳本與除錯**——在編輯器或遊戲中執行 GDScript，將長時間執行的遊戲腳本作為背景工作啟動，讀取外掛/遊戲/腳本錯誤日誌（例如 `execute_script`、`execute_game_script`、`start_game_job`、`get_plugin_log`）。
- **專案與知識**——讀取、搜尋與複製專案檔案與目錄，檢視專案設定，離線查詢引擎內建的 API 文件（例如 `read_file`、`find_in_files`、`list_directory`、`copy_file`、`get_project_settings`、`get_docs_class`）。
- **試玩與視覺驗證**——啟動目前場景，注入定時輸入序列，為編輯器與遊戲截圖，並比對前後差異（例如 `play_editor_current_scene`、`queue_game_input`、`capture_editor_viewport`、`capture_game_viewport`、`review_scene_visually`）。

## 快速開始

需要 **Godot 4.7+**，並使用圖形化編輯器工作階段。

1. **安裝**——在 Godot 專案根目錄執行一鍵腳本，或依 [INSTALL.md](INSTALL.md) 採手動 Release 安裝與原始碼建置途徑。
   - macOS/Linux：
     ```bash
     curl -fsSL https://raw.githubusercontent.com/jesspig/Godot-Autopilot/master/scripts/install.sh \
       | bash -s -- --project "$PWD" --skills
     ```
   - Windows PowerShell：
     ```powershell
     & ([scriptblock]::Create((irm https://raw.githubusercontent.com/jesspig/Godot-Autopilot/master/scripts/install.ps1))) `
       -Project (Get-Location).Path -Skills
     ```
   腳本會安裝 `gda` CLI、偵測 `project.godot`、安裝外掛，並可選擇安裝技能書。
2. **開啟你的專案**——在圖形化編輯器工作階段中，伺服器會自動啟動，並於首次啟動時綁定一個空閒的迴環連接埠（依專案持久化，選自 `20000–29999`）。無頭/命令列編輯器工作階段不會啟動伺服器。
3. **連接你的 AI 客戶端**——執行 `gda status --json`，並將其 `mcp_url` 加入客戶端的 MCP 設定（或把 URL 交給你的代理，請它產生設定）。任何支援 Streamable HTTP 的 MCP 客戶端都能連接；例如 Claude Code、Codex、Cursor、OpenCode 與 Copilot。
4. **提出需求**——例如「建立一個能跑能跳的玩家角色，接著試玩並修正看起來不對的地方」。

高風險工具（`destructive`、`process`、`code_execute`、`game_runtime`）需要 `confirm=true`；沒有全域允許清單。

## 讓 AI 代理安裝

把下面這段交給能在同一台機器上存取 shell 的代理：

```text
閱讀 https://github.com/jesspig/Godot-Autopilot/blob/master/INSTALL.md 並安裝。
```

代理會找出 Godot 專案、安裝 CLI 與外掛、詢問技能書要裝在全域或僅限專案，並以 `gda status --json` 驗證連線，最後回報 MCP URL。完整的代理規則見 [INSTALL.md](INSTALL.md)。

## 技能書

外掛內建 **10 冊自包含技能書**（總覽，以及場景、資源、腳本、執行階段、伺服器、內容、C#、安裝與工具分冊）。擇一作用域安裝：

```bash
# 全域：所有 Godot 專案共用
gda install skills

# 專案內：隨此專案保存
gda install skills --path "<project>/.agents/skills"

# 中文版
gda install skills --zh
gda install skills --path "<project>/.agents/skills" --zh
```

預設目標是 `~/.agents/skills`。若客戶端不讀取該目錄，可改用 `--path <client-skills-dir>`。技能書會教導呼叫協定，而不是讓代理自行摸索。定位：MCP 是主要產品通道，技能書是體驗層；當 MCP 無法使用時，呼叫指南會改走 GDA CLI。

外掛隨附一個編輯器面板：

- **Log**——僅含外掛日誌，支援層級/分類篩選與搜尋，方便追查行為異常的呼叫。

## 代理的使用方式

你不需要記住工具名稱。代理會依循一個簡單的循環：

1. **探索**——用 `search_tools` 找出候選工具，用 `get_tool_detail` 確認參數（切勿猜測名稱）。
2. **執行**——單一步驟用 `call_tool`，批次用 `batch_execute`。
3. **驗證**——截圖查看結果，與上一次截圖比對，修正後再重複。

## 命令列存取

比起聊天客戶端，更偏好終端機？GDA CLI 讓你在 shell 中驅動執行中的 GDA 實例——不需要整合 MCP 客戶端。它以 Bun + TypeScript 撰寫、直接從原始碼執行，具備**零執行階段依賴**且無建置步驟。

用 `bunx` 單次執行（在 PowerShell 下需加引號，因為 `#` 會開始註解）：

```bash
bunx "github:jesspig/Godot-Autopilot#0.3.3" ping
```

或全域安裝一次，之後使用 `gda` 命令：

```bash
bun install -g github:jesspig/Godot-Autopilot#0.3.3
gda ping
```

命令族：**呼叫**——`ping` / `status` / `categories` / `search` / `describe` / `call`；**生命週期**——`list` / `start` / `stop` / `restart` / `logs`；**離線與資產**——`skills` / `tools` / `projects` / `cache` / `install` / `update` / `trace`。

需要 **Bun**；若機器上沒有 Bun 或 Node，請改用 Releases 頁面的 `gda` 獨立二進位檔（完整參考見 [docs/wiki/modules/cli.md](docs/wiki/modules/cli.md)）。

## 安全性說明

伺服器只監聽你機器的迴環位址（連接埠由外掛依專案選擇，路徑固定為 `/mcp`），並拒絕非本機位址。高風險工具（`destructive`、`process`、`code_execute`、`game_runtime`）需要 `confirm=true`——沒有全域允許清單。詳見 [docs/wiki/security_contract.md](docs/wiki/security_contract.md)。

## 文件

- [INSTALL.md](INSTALL.md)——一鍵腳本、手動 Release 安裝、原始碼建置與代理安裝說明。
- [docs/wiki/index.md](docs/wiki/index.md)——專案知識庫：架構、模組、測試與慣例。
- [docs/wiki/build.md](docs/wiki/build.md)——建置、部署與封裝。
- [docs/wiki/modules/cli.md](docs/wiki/modules/cli.md)——完整 CLI 參考。
- [tests/README.md](tests/README.md)——測試結構與命令。

## 素材聲明

`examples/` 下的 5 個範例專案（2D/3D 入門教學、UI 控制項展示、視埠合成、連線 Pong）來自官方 [Godot 示範專案集](https://github.com/godotengine/godot-demo-projects)，僅保留其素材與乾淨的專案設定供測試使用。第六個專案 `examples/pink_man_adventure` 是從歷史上的 `Example/` 示範專案還原的倉庫內範例：其 173 個 PNG 素材來自 Pixel Frog 的 Pixel Adventure 1（CC0 1.0），設計文件則為本倉庫原創。版權與授權細節見各專案的 README 與 LICENSE 檔案。

## 授權

MIT
