#!/bin/bash
set -e

# Linux build script for HXPainter

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

echo "Building HXPainter in $BUILD_DIR..."

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -G Ninja -DCMAKE_BUILD_VERSION=0.1.0
ninja

echo "Build complete."
echo "You can run the application with: ./scripts/run_linux.sh"
