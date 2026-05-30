#!/bin/bash
set -e

# Linux setup script for HXPainter dependencies (Ubuntu/Debian)

echo "Installing build dependencies for HXPainter..."

if [ -f /etc/debian_version ]; then
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        ninja-build \
        qt6-base-dev \
        qt6-svg-dev \
        qt6-opengl-dev \
        libgl1-mesa-dev \
        python3
else
    echo "Warning: This script is intended for Debian/Ubuntu-based systems."
    echo "Please ensure you have C++20 compiler, CMake, Ninja, and Qt 6.7+ (Widgets, OpenGL, SVG) installed."
fi

echo "Setup complete. You can now run ./scripts/build_linux.sh"
