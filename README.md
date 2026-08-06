# Godot-Self-Driving

> **An MCP Server for Godot Engine — AI-native engine control at the API level**

[中文版说明](README_zh.md)

Godot-Self-Driving is an [MCP (Model Context Protocol)](https://modelcontextprotocol.io/) server that bridges AI agents with the Godot game engine at the **native engine API level**. Unlike conventional tools that operate from a user-UI perspective (simulating clicks or editor operations), this project gives AI agents direct, programmatic access to Godot's entire engine surface — scene tree manipulation, physics servers, rendering servers, audio, navigation, input simulation, script execution, and more.

This is an **in-process GDExtension plugin** that loads directly into the Godot editor. No standalone bridge process needed — the MCP server starts when your project opens, and stops when it closes.

## Architecture

```
MCP Host (Claude Desktop, Cursor, etc.)
  │ POST http://127.0.0.1:9527/mcp
  ▼
Godot Editor
  └── Godot-Self-Driving (GDExtension)
      ├── libhv (internal HTTP threads)
      ├── mcp-cpp-sdk: McpServer + Streamable HTTP
      ├── Command Queue (libhv → Godot main thread bridge)
      ├── ~355 MCP Tools across 22 categories
      ├── Inline Documentation (offline engine docs)
      └── Custom Log Dock (dedicated plugin output panel)
```

### Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **Transport** | Streamable HTTP (POST /mcp) | Standard MCP protocol, no bridge process |
| **Thread Model** | Command queue + frame sync | Safe Godot main-thread-only API access |
| **Port** | 9527 | Configurable via `GODOT_SELF_DRIVING_PORT` env var |
| **Discovery** | 3-Tier Progressive (Catalog→Inspect→Execute) | Keeps context small with ~355 tools |
| **Search** | BM25 keyword | Tools organized by namespace + descriptions |
| **Build** | CMake 3.28+ / C++17 | Cross-platform, auto-optimized builds |

## Features

### 🎮 Full Engine Control (~355 Tools)

| Category | Tools | Description |
|----------|:-----:|-------------|
| **Physics** | 51 | 2D/3D ray casts, body creation, force application, joints |
| **Render** | 50 | Canvas items, cameras, lights, meshes, viewports, materials |
| **Editor** | 25 | Selection, undo/redo, scene save, plugin management |
| **Display** | 24 | Window, viewport and screen properties |
| **Debug** | 23 | Performance monitors, profiling, diagnostics |
| **Resources** | 22 | Load, save, create, list resources |
| **Audio** | 20 | Bus management, stream playback, effects |
| **Input** | 17 | Key/mouse/gamepad simulation, action queries |
| **OS** | 15 | Operating system, environment and clipboard access |
| **Navigation** | 15 | Nav mesh, path queries, agents |
| **Config** | 13 | Project settings, engine properties |
| **Scene** | 13 | Node creation, deletion, scene tree inspection (e.g. `scene_node_create`) |
| **Text** | 11 | String manipulation, parsing and formatting |
| **Scripts** | 9 | Execute GDScript and C#, call methods on any node |
| **Debugger** | 7 | Debugger session control and inspection |
| **TileMap** | 7 | Tile map creation, cell manipulation and queries |
| **Properties** | 5 | Get/set properties, list properties, signal connect (e.g. `property_set`) |
| **Docs** | 4 | Query offline Godot API docs |
| **Game** | 3 | Game loop control and engine-wide state |
| **Group** | 3 | Node group management and membership queries |
| **SpriteFrames** | 3 | Sprite frame set creation and animation management |
| **System** | 1 | Plugin-level system information |

### 📖 Inline API Documentation

Query Godot's built-in offline documentation directly through MCP tools. No web searches needed — every class, method, property, and signal is documented from the engine's own `DocTools` cache:

- `doc_get_class` — Full class docs (description, methods, properties, signals)
- `doc_search` — Search classes by name or keyword
- `doc_get_method` — Method signature and description
- `doc_get_property` — Property type and description

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
git clone https://github.com/jesspig/Godot-Self-Driving.git
cd Godot-Self-Driving

# Recommended: build + deploy to Example/addons/ in one step
uv run build.py             # Debug
uv run build.py --release   # Release (cleans first)

# Or manual CMake (presets: debug, release, both Ninja)
cmake --preset release && cmake --build --preset release
```

The built `.dll` / `.so` / `.dylib` will be in `build/release/`. `build.py` also generates the `.gdextension` file and copies artifacts to `Example/addons/godot-self-driving/`.

### Install

Copy to your Godot project:

```
your-project/
└── addons/
    └── godot-self-driving/
        ├── godot-self-driving.dll      (or .so / .dylib)
        └── godot-self-driving.gdextension
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
| **HTTP / Async** | libhv (internal) |
| **JSON** | mcp::JsonValue (SDK built-in) |
| **Build** | CMake 3.28+ / C++17 |
| **Optimization** | Clang-first, ThinLTO, Ninja, sccache, Unity Build |

## Asset Attribution

The `Example/` test project uses pixel art assets from [Pixel Adventure 1](https://pixelfrog-assets.itch.io/pixel-adventure-1) by Pixel Frog.

## License

MIT
