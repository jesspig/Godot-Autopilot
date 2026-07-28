# Godot-Self-Driving

> **An MCP Server for Godot Engine — AI-native engine control at the API level**

[中文版说明](README_zh.md)

Godot-Self-Driving is an [MCP (Model Context Protocol)](https://modelcontextprotocol.io/) server that bridges AI agents with the Godot game engine at the **native engine API level**. Unlike conventional tools that operate from a user-UI perspective (simulating clicks or editor operations), this project gives AI agents direct, programmatic access to Godot's entire engine surface — scene tree manipulation, physics servers, rendering servers, audio, navigation, input simulation, script execution, and more.

This is an **in-process GDExtension plugin** that loads directly into the Godot editor or runtime. No standalone bridge process needed — the MCP server starts when your project opens, and stops when it closes.

## Architecture

```
MCP Host (Claude Desktop, Cursor, etc.)
  │ POST http://127.0.0.1:9527/mcp
  ▼
Godot Editor / Runtime
  └── Godot-Self-Driving (GDExtension)
      ├── libhv (internal HTTP threads)
      ├── mcp-cpp-sdk: McpServer + Streamable HTTP
      ├── Command Queue (libhv → Godot main thread bridge)
      ├── ~239 MCP Tools across 13 categories
      ├── Inline Documentation (offline engine docs)
      └── Custom Log Dock (dedicated plugin output panel)
```

### Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **Transport** | Streamable HTTP (POST /mcp) | Standard MCP protocol, no bridge process |
| **Thread Model** | Command queue + frame sync | Safe Godot main-thread-only API access |
| **Port** | 9527 (auto +1 on conflict) | Configurable via ProjectSettings or env |
| **Discovery** | 3-Tier Progressive (Catalog→Inspect→Execute) | Keeps context small with 244 tools |
| **Sandbox** | QuickJS (async/await) | Compose multi-tool flows in a single script |
| **Search** | BM25 keyword | Tools organized by namespace + descriptions |
| **Build** | CMake 3.28+ / C++17 | Cross-platform, auto-optimized builds |

## Features

### 🎮 Full Engine Control (~244 Tools)

| Category | Tools | Description |
|----------|:-----:|-------------|
| **scene** | 30 | Node creation, deletion, reparenting, scene management |
| **property** | 15 | Get/set properties, call methods, signal connect/emit |
| **resource** | 20 | Load, save, create, list resources |
| **physics** | 40 | 2D/3D ray casts, body creation, force application, joints |
| **render** | 30 | Canvas items, cameras, lights, meshes, viewports, materials |
| **navigation** | 15 | Nav mesh, path queries, agents |
| **audio** | 15 | Bus management, stream playback, effects |
| **input** | 10 | Key/mouse/gamepad simulation, action queries |
| **script** | 10 | Execute GDScript and C#, call methods on any node |
| **editor** | 20 | Selection, undo/redo, scene save, plugin management |
| **config** | 15 | Project settings, engine properties |
| **debug** | 15 | Performance monitors, profiling, diagnostics |
| **documentation** | 4 | Query offline Godot API docs |

### 📖 Inline API Documentation

Query Godot's built-in offline documentation directly through MCP tools. No web searches needed — every class, method, property, and signal is documented from the engine's own `DocTools` cache:

- `documentation.get_class` — Full class docs (description, methods, properties, signals)
- `documentation.search` — Search classes by name or keyword
- `documentation.get_method` — Method signature and description
- `documentation.get_property` — Property type and description

### 🧩 Programmatic Composition (QuickJS Sandbox)

Write JavaScript to compose multiple engine operations in one request. Data flows between tools within the sandbox — only the summary reaches the model context. Async/await fully supported.

```javascript
// Example: Batch-create a scene setup
const root = await scene.node.create({type: "Node3D", name: "World"});
const light = await scene.node.create({type: "DirectionalLight3D", name: "Sun", parent: root.path});
await property.set({path: light.path + ":position", value: {x: 5, y: 10, z: 5}});
console.log(`Scene ready: ${root.path}`);
```

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
```

### 📝 Dedicated Log Panel

A custom `EditorDock` bottom panel displays plugin-only logs (system, tools, sandbox, transport) with:

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
git clone --recursive https://github.com/your-org/Godot-Self-Driving.git
cd Godot-Self-Driving
cmake --preset release
cmake --build --preset release
```

The built `.dll` / `.so` / `.dylib` will be in `build/release/`.

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

## License

MIT
