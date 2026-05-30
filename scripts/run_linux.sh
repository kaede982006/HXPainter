#!/bin/bash
set -e

# Linux run script for HXPainter

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
EXE="$BUILD_DIR/HXPainter"

if [ ! -f "$EXE" ]; then
    echo "Error: Executable not found at $EXE. Please run ./scripts/build_linux.sh first."
    exit 1
fi

echo "Running HXPainter..."
cd "$BUILD_DIR"
./HXPainter "$@"
