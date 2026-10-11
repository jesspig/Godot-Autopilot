# Godot-Autopilot

[![라이선스](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![빌드](https://github.com/jesspig/Godot-Autopilot/actions/workflows/ci.yml/badge.svg)](https://github.com/jesspig/Godot-Autopilot/actions)
![Godot](https://img.shields.io/badge/Godot-4.7%2B-478CBF?logo=godotengine&logoColor=white)
![플랫폼](https://img.shields.io/badge/platform-Windows%7CLinux%7CmacOS-lightgrey)
[![프로토콜](https://img.shields.io/badge/MCP-Streamable_HTTP-blueviolet)](https://modelcontextprotocol.io/)
![도구 수](https://img.shields.io/badge/tools-443%2B-brightgreen)

> **AI 에이전트가 에디터 안에서 Godot 게임을 빌드하고 플레이테스트하며 디버깅합니다.**

[English](README.md) | [简体中文](README.zh-CN.md) | [繁體中文](README.zh-TW.md) | [日本語](README.ja.md) | **한국어**

Godot-Autopilot은 **Godot 에디터 내부**에서 플러그인으로 동작하는 [MCP](https://modelcontextprotocol.io/) 서버입니다. 설치가 완료되면 AI 코딩 어시스턴트(Claude Code, Cursor, Codex, OpenCode, …)가 사용자와 같은 방식으로 프로젝트를 조작합니다. 씬을 빌드하고, 속성을 조정하고, 게임을 실행하고, 버튼을 누르고, 스크린샷을 촬영하고, 로그를 읽은 뒤 확인한 내용을 바탕으로 반복 개선합니다.

브리지 프로세스가 필요하지 않으며 채팅창에 오류를 복사하여 붙여넣을 필요가 없습니다. 프로젝트를 열면 서버가 시작되고 프로젝트를 닫으면 서버가 중지됩니다.

## 엔드투엔드 데모

엔드투엔드 테스트 중 실시간으로 게임 캐릭터를 조작하는 AI:

![Demo 001](assets/demo-001.gif)

## 주요 기능

**443개 이상의 도구**로 에디터부터 게임까지 전체 과정을 지원합니다(정확한 수는 버전마다 증가하므로 `search_tools`를 통해 에이전트에게 확인하도록 요청하십시오). 주요 기능:

- **씬 빌드** — 노드를 생성, 이름 변경, 이동, 삭제합니다. 속성을 읽고 쓰며, 시그널을 연결하고, GridMap으로 3D 블록 레벨을 구축합니다(예: `create_scene_node`, `property_set`, `signal_connect`, `scene_tree_items`; GridMap: `create_gridmap`, `set_gridmap_cell`, `fill_gridmap_box`).
- **외관, 사운드 및 질감** — 메시와 머티리얼, 조명과 환경, 오디오 재생, 애니메이션과 트윈, UI 테마, 타일맵을 지원합니다(예: `create_animation`, `tween_editor_property`, `create_game_tween`, `play_audio_player`, `fill_tilemap_rect`).
- **물리와 내비게이션** — 레이/셰이프 캐스트, 리지드바디, 내비게이션 맵과 경로 조회를 지원합니다(예: `intersect_physics_3d_ray`, `create_nav_3d_map`, `get_nav_3d_map_path`).
- **손: 입력과 UI 자동화** — 키, 마우스, 게임패드를 시뮬레이션하고, 이름으로 에디터 컨트롤을 클릭하고, 에디터 단축키를 실행하며, 실행 중인 게임의 버튼을 클릭합니다(예: `click_input_mouse`, `get_editor_ui_elements`, `click_editor_element`, `run_editor_shortcut`, `click_game_ui_element`).
- **스크립트와 디버깅** — 에디터 또는 게임에서 GDScript를 실행하고, 시간이 오래 걸리는 게임 스크립트를 백그라운드 작업으로 실행하며, 플러그인/게임/스크립트 오류 로그를 읽습니다(예: `execute_script`, `execute_game_script`, `start_game_job`, `get_plugin_log`).
- **프로젝트와 지식** — 프로젝트 파일과 디렉터리를 읽고, 검색하고, 복사하며, 프로젝트 설정을 확인하고, 엔진 내장 API 문서를 오프라인으로 조회합니다(예: `read_file`, `find_in_files`, `list_directory`, `copy_file`, `get_project_settings`, `get_docs_class`).
- **플레이와 시각적 검증** — 현재 씬을 실행하고, 시간 지정 입력 시퀀스를 주입하며, 에디터와 게임의 스크린샷을 촬영하고 전후 차이를 비교합니다(예: `play_editor_current_scene`, `queue_game_input`, `capture_editor_viewport`, `capture_game_viewport`, `review_scene_visually`).

## 에이전트 사용 방식

도구 이름을 외울 필요가 없습니다. 에이전트는 다음과 같은 간단한 과정을 따릅니다.

1. **탐색** — `search_tools`로 후보를 찾고 `get_tool_detail`로 매개변수를 확인합니다(이름을 추측하지 마십시오).
2. **실행** — 단일 단계에는 `call_tool`, 일괄 처리에는 `batch_execute`를 사용합니다.
3. **검증** — 결과를 스크린샷으로 확인하고 이전 캡처와 비교한 뒤 수정하고 반복합니다.

에이전트가 바로 원활하게 동작하도록 플러그인에 **스킬북 9권**(개요, 호출 가이드, 씬, 리소스, 스크립팅, 런타임, 서버, 콘텐츠, C# 편)과 **시작 가이드 7편**(3D 씬, 캐릭터 컨트롤러, 물리 디버깅, 입력 맵, GUI, …)을 포함합니다. **MCP Config** 패널에서 한 번 클릭하면 프로젝트의 `.agents/skills/`에 스킬이 생성됩니다. MCP를 주 제품 채널로 삼고 스킬/스크립트를 경험 계층으로 둡니다. 호출 가이드는 MCP가 중단될 경우 스크립트 브리지로 대체되며(Node 18+ 또는 Bun 필요), 스크립트 런타임이 없을 경우에는 수동 가이드로 대체됩니다.

플러그인에는 두 개의 에디터 패널이 포함됩니다.

- **MCP Config** — 서버 상태와 포트, 한 번의 클릭으로 끝나는 클라이언트 설정, 기능 스위치, 스킬 생성을 지원합니다.
- **Log** — 레벨/분류 필터와 검색 기능을 갖춘 플러그인 전용 로그이므로 비정상 호출을 쉽게 추적합니다.

## 시작하기

**Godot 4.7+**가 필요합니다.

1. **플러그인 가져오기** — 프로젝트의 GitHub 릴리스 페이지([jesspig/Godot-Autopilot](https://github.com/jesspig/Godot-Autopilot))에서 `godot-autopilot-<version>.zip`을 다운로드한 뒤 `godot-autopilot/` 폴더를 프로젝트의 `addons/` 디렉터리에 복사합니다.
   ```
   your-project/
   └── addons/
       └── godot-autopilot/
           ├── godot-autopilot.dll      (Windows)
           ├── libgodot-autopilot.so    (Linux)
           ├── libgodot-autopilot.dylib (macOS)
           └── godot-autopilot.gdextension
   ```
2. **프로젝트 열기** — 플러그인 설정의 포트(기본값 `http://127.0.0.1:9527/mcp`)에서 서버가 자동으로 시작됩니다. 별도의 활성화 스위치는 필요하지 않습니다.
3. **AI 클라이언트 연결** — 에디터의 **MCP Config** 패널에서 클라이언트를 선택하고 **Generate**를 클릭합니다. Claude Code, Codex, Cursor, OpenCode, Copilot을 포함한 20개 클라이언트를 지원합니다. 클라이언트가 요청하면 생성된 설정 파일을 승인합니다.
4. **강력한 기능 잠금 해제(선택 사항)** — 스크립트 실행(`code_execute`)과 게임 제어(`game_runtime`)는 기본적으로 꺼져 있습니다. 필요할 때 패널에서 선택합니다.
5. **요청하기** — 예: "달리고 점프할 수 있는 플레이어 캐릭터를 만들고, 플레이테스트한 뒤 어색한 부분을 수정하십시오."

소스에서 빌드하거나 기여하고 싶으십니까? [docs/wiki/](docs/wiki/)([index.md](docs/wiki/index.md)부터 시작하고 빌드는 [build.md](docs/wiki/build.md) 참조) 및 [tests/README.md](tests/README.md)를 참조하십시오.

## 안전 참고 사항

서버는 컴퓨터의 루프백 주소에서만 수신 대기하며(포트는 플러그인 설정값이며 기본값은 9527, 고정 경로는 `/mcp`), 로컬이 아닌 주소는 거부합니다. 스크립트 실행, 게임 제어, OS 프로세스 도구는 기본적으로 **거부**됩니다. 신뢰할 수 있는 로컬 프로젝트에서만 활성화하십시오. 자세한 내용: [docs/wiki/security_contract.md](docs/wiki/security_contract.md).

## 에셋 출처 표기

`demo/` 아래 5개 예제 프로젝트(2D/3D 입문 튜토리얼, UI 컨트롤 갤러리, 뷰포트 구성, 네트워크 퐁)는 공식 [Godot 데모 프로젝트](https://github.com/godotengine/godot-demo-projects)에서 가져왔으며, 테스트용으로 에셋과 정리된 프로젝트 설정만 유지합니다. 저작권과 라이선스 세부 정보는 각 프로젝트의 README와 LICENSE 파일에 있습니다.

## 라이선스

MIT