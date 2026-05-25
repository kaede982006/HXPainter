# HXPainter

HXPainter is a C++/Qt painting application MVP based on the project draft. It now has a conventional painting workspace: menu bar, top toolbar, left tool palette, OpenGL-backed canvas, right-side docks, layer-local painting/erasing, command-based undo/redo, `.hxp` project save/load, image import, and PNG/JPG export.

## Build

Install Qt 6.7.3, MinGW 13.1.0, CMake, and Ninja into the project-local `.qt` folder:

```powershell
.\scripts\setup_qt.ps1
```

Build and run:

```powershell
.\scripts\build.ps1
.\scripts\run.ps1
```

Run the smoke tests after a build:

```powershell
ctest --test-dir build --output-on-failure
```

## Deploy

Create a standalone Windows distribution with:

```powershell
.\scripts\deploy_windows.ps1
```

The deployment output is `dist\HXPainter`. It contains `HXPainter.exe`, `logo.png`, Qt DLLs, and Qt plugin folders such as `platforms`.

Generated build, Qt SDK, deployment, and log outputs are intentionally excluded from source distribution.

Bundled SVG icons use Tabler Icons. Keep `THIRD_PARTY_NOTICES.txt` with source distributions.
