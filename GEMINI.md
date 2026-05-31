# HXPainter

HXPainter is a C++/Qt painting application MVP. It provides a conventional painting workspace with a layer-based document system, QWidget-backed canvas, and various drawing tools.

## Project Overview

- **Core Technologies:** C++20, Qt 6.7.3 (Widgets, SVG), CMake, Ninja.
- **Architecture:** 
    - **Document Model:** `CanvasDocument` manages layers (`Layer`) and document settings.
    - **Rendering:** `OpenGLCanvasWidget` is a QWidget-backed canvas renderer. The class name is historical; the current build does not link Qt OpenGL modules.
    - **Tool System:** `BrushEngine`, `EraserEngine`, `FillEngine`, and `TextEngine` implement the core drawing logic.
    - **Command Pattern:** `CommandManager` provides undo/redo support via `Command` objects and document snapshots.
    - **Input:** `TabletInputMapper` handles mouse, tablet, and touch events.

## Directory Structure

- `HXPainter/src/`: Main source code.
    - `actions/`: Action registration and management.
    - `app/`: Application-wide utilities (paths, icons).
    - `brush/`: Drawing engine implementations.
    - `commands/`: Undo/redo command logic.
    - `core/`: Basic data structures and performance tracking.
    - `document/`: High-level document and layer management.
    - `export/`: Image export functionality (PNG, JPG).
    - `input/`: Input event mapping and handling.
    - `render/`: Canvas rendering and document state.
    - `serialization/`: `.hxp` project file save/load (JSON-based).
    - `tools/`: Tool state and management.
    - `ui/`: Qt Widgets and UI components.
- `HXPainter/resources/`: Qt resource files (QSS themes, SVG icons).
- `HXPainter/scripts/`: PowerShell and Python scripts for setup, build, and deployment.
- `krita/`: (External Reference) Mirror of the Krita painting application repository.

## Building and Running

### Prerequisites
- Qt 6.7.3 with Widgets and SVG
- MinGW 13.1.0
- CMake 3.21+
- Ninja
- Python 3 (for Windows icon generation)
- Wine (optional, for Linux-hosted Windows smoke tests)

### Commands
The project uses a unified management script `manage.sh` on a Linux host for all build and setup tasks.

#### Setup
- **Install Dependencies:**
  ```bash
  ./manage.sh setup
  ```
  *This installs compilers, Qt6 development libraries, and MinGW for cross-compilation.*

#### Building
- **GenBuild (Linux Native):**
  ```bash
  ./manage.sh gen
  ```
- **WinBuild (Windows Cross-compile):**
  ```bash
  ./manage.sh win
  ```

#### Running & Cleaning
- **Run Linux Build:**
  ```bash
  ./manage.sh run
  ```
- **Clean Build Artifacts:**
  ```bash
  ./manage.sh clean
  ```

#### Testing
- **Run Tests:**
  ```bash
  ./manage.sh test
  ./manage.sh test-win
  ```

  The executable supports several smoke test flags:
  - `--smoke-icon-test`
  - `--mvp-smoke-test`
  - `--theme-smoke-test`
  - `--startup-smoke-test`
  - `--functional-regression-smoke-test`

## Development Conventions

- **Standard:** C++20 (`CMAKE_CXX_STANDARD 20`).
- **Qt Keywords:** `QT_NO_KEYWORDS` is enabled. Use `Q_EMIT`, `Q_SLOT`, `Q_SIGNALS`, etc.
- **Naming:**
    - Classes: `PascalCase` (e.g., `CanvasDocument`).
    - Methods/Variables: `camelCase` (e.g., `activeLayerIndex`).
    - Private Members: Trailing underscore (e.g., `state_`).
- **Error Handling:** Use return codes and `QString *error` parameters for diagnostic messages.
- **Persistence:** Project files (`.hxp`) use a JSON-based format.
