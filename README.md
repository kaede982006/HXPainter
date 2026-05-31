# HXPainter

HXPainter is a C++20/Qt 6 painting application MVP. It has a conventional painting workspace with a menu bar, top toolbar, left tool palette, QWidget-backed canvas, right-side docks, layer-local painting/erasing, command-based undo/redo, `.hxp` project save/load, image import, and PNG/JPG export.

## Build

On Windows, install Qt 6.7.3, MinGW 13.1.0, CMake, and Ninja into the project-local `.qt` folder:

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

On a Linux build host, `manage.sh` drives the native and Windows cross-builds:

```bash
./manage.sh gen
./manage.sh win
./manage.sh test-win
```

The smoke-test flags are `--smoke-icon-test`, `--mvp-smoke-test`, `--theme-smoke-test`, `--startup-smoke-test`, and `--functional-regression-smoke-test`.

## Deploy

Create a standalone Windows distribution with:

```powershell
.\scripts\deploy_windows.ps1
```

The deployment output is `dist\HXPainter`. It contains `HXPainter.exe`, `logo.png`, Qt DLLs, and Qt plugin folders such as `platforms`, `imageformats`, `iconengines`, and `styles`. The current canvas path uses Qt Widgets and does not require Qt OpenGL runtime DLLs.

Generated build, Qt SDK, deployment, and log outputs are intentionally excluded from source distribution.

Bundled SVG icons use Tabler Icons. Keep `THIRD_PARTY_NOTICES.txt` with source distributions.
