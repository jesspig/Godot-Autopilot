# Godot-Autopilot

> **An MCP Server for Godot Engine — AI-native engine control at the API level**

[中文版说明](README_zh.md)

Godot-Autopilot is an [MCP (Model Context Protocol)](https://modelcontextprotocol.io/) server that bridges AI agents with the Godot game engine at the **native engine API level**. Unlike conventional tools that operate from a user-UI perspective (simulating clicks or editor operations), this project gives AI agents direct, programmatic access to Godot's entire engine surface — scene tree manipulation, physics servers, rendering servers, audio, navigation, input simulation, script execution, and more.

This is an **in-process GDExtension plugin** that loads directly into the Godot editor. No standalone bridge process needed — the MCP server starts when your project opens, and stops when it closes.

## Architecture

```
MCP Host (Claude Desktop, Cursor, etc.)
  │ POST http://127.0.0.1:9527/mcp
  ▼
Godot Editor
  └── Godot-Autopilot (GDExtension)
      ├── mcp-cpp-sdk: HTTP server (internal threads)
      ├── mcp-cpp-sdk: McpServer + Streamable HTTP
      ├── Command Queue (HTTP thread → Godot main thread bridge)
      ├── ~384 MCP Tools across 27 categories (count varies by plugin version; see MCP search_tools)
      ├── Inline Documentation (offline engine docs)
      └── Custom Log Dock (dedicated plugin output panel)
```

### Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **Transport** | Streamable HTTP (POST /mcp) | Standard MCP protocol, no bridge process |
| **Thread Model** | Command queue + frame sync | Safe Godot main-thread-only API access |
| **Port** | 9527 | Configurable via `GODOT_AUTOPILOT_PORT` env var |
| **Discovery** | 3-Tier Progressive (Catalog→Inspect→Execute) | Keeps context small with ~384 tools (count varies by plugin version) |
| **Search** | BM25 keyword | Tools organized by namespace + descriptions |
| **Build** | CMake 3.28+ / C++17 | Cross-platform, auto-optimized builds |

## Features

### 🎮 Full Engine Control (~384 Tools, count varies by plugin version; see MCP search_tools)

| Category | Tools | Description |
|----------|:-----:|-------------|
| **Render** | 49 | Canvas items, cameras, lights, meshes, viewports, materials |
| **Physics** | 47 | 2D/3D ray casts, body creation, force application, joints |
| **Resources** | 26 | Load, save, create, copy, reload and list resources |
| **Display** | 25 | Window, viewport and screen properties |
| **Editor** | 31 | Selection, undo/redo, scene save, plugin management |
| **Audio** | 20 | Bus management, stream playback, effects |
| **Input** | 23 | Key/mouse/gamepad simulation, action queries (includes InputMap) |
| **OS** | 18 | Operating system, environment and clipboard access |
| **Debug** | 16 | Performance monitors, profiling, diagnostics |
| **Navigation** | 15 | Nav mesh, path queries, agents |
| **Scene** | 15 | Node creation, deletion, scene tree inspection (e.g. `create_scene_node`) |
| **Config** | 13 | Project settings, engine properties |
| **Text** | 10 | String manipulation, parsing and formatting |
| **Animation** | 10 | Animation players, tracks, mixers and playback |
| **Scripts** | 10 | Execute GDScript and C#, call methods on any node |
| **Game** | 12 | Game loop control and engine-wide state |
| **Theme** | 8 | Theme resources, style boxes and font variations |
| **TileMap** | 8 | Tile map creation, cell manipulation and queries |
| **Debugger** | 6 | Debugger session control and inspection |
| **Properties** | 5 | Get/set properties, list properties, signal connect (e.g. `property_set`) |
| **Docs** | 4 | Query offline Godot API docs |
| **Group** | 3 | Node group management and membership queries |
| **SpriteFrames** | 3 | Sprite frame set creation and animation management |
| **Analysis** | 3 | Scene file validation and project analysis helpers |
| **Testing** | 2 | In-editor script test helpers |
| **System** | 1 | Plugin-level system information |
| **Capture** | 1 | Editor viewport screenshot |

### 📖 Inline API Documentation

Query Godot's built-in offline documentation directly through MCP tools. No web searches needed — every class, method, property, and signal is documented from the engine's own `DocTools` cache:

- `get_docs_class` — Full class docs (description, methods, properties, signals)
- `find_docs_class` — Search classes by name or keyword
- `get_docs_method` — Method signature and description
- `get_docs_property` — Property type and description

### 📋 MCP Resources

The server exposes engine state as readable MCP Resources:

```
godot://engine/version              — Engine version info
godot://scene/tree                  — Current scene node tree (JSON)
godot://scene/{path}                — Node properties by path
godot://filesystem/tree             — Project file system structure
godot://filesystem/{path}           — File/directory content
godot://editor/selection            — Current selection
godot://editor/settings/{key}       — Editor settings
godot://log/recent                  — Recent plugin log entries
```

### 📝 Dedicated Log Panel

A custom `EditorDock` bottom panel displays plugin-only logs (system, tools, transport, resources, prompts) with:

- Level filtering (Debug / Info / Warning / Error)
- Category filtering
- Text search
- Collapse duplicate messages
- Theme-consistent styling (matches Godot editor theme)

## Getting Started

### Prerequisites

- CMake 3.28+
- C++17 compiler (Clang recommended, MSVC/GCC supported)
- Godot 4.3+ with GDExtension support

### Build

```bash
git clone https://github.com/jesspig/Godot-Autopilot.git
cd Godot-Autopilot

# Recommended: build + deploy to Example/addons/ in one step
uv run build.py             # Debug
uv run build.py --release   # Release (cleans first)

# Or manual CMake (presets: debug, release, both Ninja)
cmake --preset release && cmake --build --preset release
```

The built `.dll` / `.so` / `.dylib` will be in `build/release/`. `build.py` also generates the `.gdextension` file and copies artifacts to `Example/addons/godot-autopilot/`.

### Install

Copy to your Godot project:

```
your-project/
└── addons/
    └── godot-autopilot/
        ├── godot-autopilot.dll      (or .so / .dylib)
        └── godot-autopilot.gdextension
```

### Configure MCP Host

```json
{
  "mcpServers": {
    "godot-engine": {
      "type": "streamable-http",
      "url": "http://127.0.0.1:9527/mcp"
    }
  }
}
```

Open your Godot project — the server starts automatically. The port displays in the editor status bar.

## Technology Stack

| Layer | Technology |
|-------|-----------|
| **Engine** | Godot 4.x (GDExtension) |
| **Bindings** | godot-cpp (FetchContent) |
| **MCP Protocol** | [modelcontextprotocol-cpp-sdk](https://github.com/jesspig/modelcontextprotocol-cpp-sdk) |
| **HTTP / Async** | mcp-cpp-sdk (internal, self-hosted) |
| **JSON** | mcp::JsonValue (SDK built-in) |
| **Build** | CMake 3.28+ / C++17 |
| **Optimization** | Clang-first, ThinLTO, Ninja, sccache, Unity Build |

## Asset Attribution

The `Example/` test project uses pixel art assets from [Pixel Adventure 1](https://pixelfrog-assets.itch.io/pixel-adventure-1) by Pixel Frog.

## License

MIT
