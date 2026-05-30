#!/bin/bash
set -e

# Unified management script for HXPainter
# Supports: Debian/Ubuntu, Arch Linux, Fedora

# --- Configuration ---
PROJECT_NAME="HXPainter"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$ROOT_DIR"
BUILD_DIR_LINUX="$ROOT_DIR/build-linux"
BUILD_DIR_WINDOWS="$ROOT_DIR/build-windows"

# --- Utility Functions ---
show_help() {
    echo "Usage: ./manage.sh [OPTION]"
    echo "Options:"
    echo "  setup       Install dependencies (Native & Cross-compile tools)"
    echo "  gen         GenBuild: Build for Linux and collect .so dependencies"
    echo "  win         WinBuild: Build for Windows and collect .dll dependencies"
    echo "  test        Run smoke tests (Linux build)"
    echo "  run         Run the Linux build"
    echo "  clean       Remove build directories and artifacts"
    echo "  help        Show this help message"
}

log_info() { echo -e "\e[34m[INFO]\e[0m $1"; }
log_success() { echo -e "\e[32m[SUCCESS]\e[0m $1"; }
log_warn() { echo -e "\e[33m[WARN]\e[0m $1"; }
log_error() { echo -e "\e[31m[ERROR]\e[0m $1"; exit 1; }

check_requirements() {
    local missing=()
    for cmd in cmake ninja python3; do
        if ! command -v "$cmd" >/dev/null 2>&1; then missing+=("$cmd"); fi
    done
    if [ ${#missing[@]} -ne 0 ]; then
        log_error "Missing required tools: ${missing[*]}. Please run './manage.sh setup' first."
    fi
}

ensure_sudo() {
    if [ "$EUID" -ne 0 ]; then
        log_info "This task requires sudo privileges."
        sudo -v || log_error "Failed to obtain sudo privileges."
        (while true; do sudo -n true; sleep 60; kill -0 "$$" || exit; done) 2>/dev/null &
        SUDO_KEEP_ALIVE_PID=$!
        trap 'kill $SUDO_KEEP_ALIVE_PID 2>/dev/null || true' EXIT
    fi
}

# --- Deployment Logic ---
deploy_linux_libs() {
    log_info "Collecting Linux Shared Libraries (.so)..."
    local target_dir="$BUILD_DIR_LINUX"
    local exe="$target_dir/HXPainter"
    
    if [ ! -f "$exe" ]; then
        log_error "Executable not found at $exe"
    fi

    # Use ldd to find dependencies and copy them
    # Filter for libraries from /usr or /lib to avoid system-critical low-level libs if desired, 
    # but for true portability we often want most of them.
    # We exclude very basic system libs like ld-linux, libc, etc. to avoid conflicts on other distros.
    ldd "$exe" | grep "=> /" | awk '{print $3}' | while read -r lib; do
        libname=$(basename "$lib")
        # Skip very low-level system libraries that should come from the host OS
        if [[ "$libname" =~ ^(libc\.so|libpthread\.so|libdl\.so|libm\.so|librt\.so|libgcc_s\.so|libstdc\+\+\.so|ld-linux) ]]; then
            continue
        fi
        cp -n "$lib" "$target_dir/" 2>/dev/null || true
    done

    # Deploy Qt Plugins (Platforms is critical)
    log_info "Deploying Qt platforms plugin (Linux)..."
    local qt_plugin_path=$(qtpaths6 --plugin-dir 2>/dev/null || echo "/usr/lib/qt6/plugins")
    mkdir -p "$target_dir/platforms"
    if [ -f "$qt_plugin_path/platforms/libqxcb.so" ]; then
        cp -n "$qt_plugin_path/platforms/libqxcb.so" "$target_dir/platforms/"
    fi
    
    mkdir -p "$target_dir/imageformats"
    if [ -f "$qt_plugin_path/imageformats/libqsvg.so" ]; then
        cp -n "$qt_plugin_path/imageformats/libqsvg.so" "$target_dir/imageformats/"
    fi

    log_success "Linux libraries collected in $target_dir"
    log_info "Note: To run with local libs, use: LD_LIBRARY_PATH=. ./HXPainter"
}

deploy_windows_dlls() {
    log_info "Collecting Windows DLLs (MinGW)..."
    local target_dir="$BUILD_DIR_WINDOWS"
    local mingw_bin="/usr/x86_64-w64-mingw32/bin"
    
    if [ ! -d "$mingw_bin" ]; then
        log_warn "MinGW bin directory not found at $mingw_bin. Skipping DLL collection."
        return
    fi

    local dlls=(
        "libgcc_s_seh-1.dll" "libstdc++-6.dll" "libwinpthread-1.dll" "libssp-0.dll"
        "Qt6Core.dll" "Qt6Gui.dll" "Qt6Widgets.dll" "Qt6OpenGL.dll" "Qt6OpenGLWidgets.dll" "Qt6Svg.dll"
        "libpng16-16.dll" "zlib1.dll" "libharfbuzz-0.dll" "libintl-8.dll" "libglib-2.0-0.dll"
        "libiconv-2.dll" "libpcre2-16-0.dll" "libpcre2-8-0.dll" "libdouble-conversion.dll" "libicuin78.dll"
        "libicuuc78.dll" "libicudt78.dll" "libzstd.dll" "libfreetype-6.dll" "libgraphite2.dll"
        "libbrotlidec.dll" "libbrotlicommon.dll" "libbz2-1.dll"
    )

    for dll in "${dlls[@]}"; do
        if [ -f "$mingw_bin/$dll" ]; then
            cp -n "$mingw_bin/$dll" "$target_dir/" 2>/dev/null || true
        fi
    done

    mkdir -p "$target_dir/platforms"
    local platform_dll="/usr/x86_64-w64-mingw32/lib/qt6/plugins/platforms/qwindows.dll"
    if [ -f "$platform_dll" ]; then cp -n "$platform_dll" "$target_dir/platforms/"; fi
    
    mkdir -p "$target_dir/imageformats"
    local svg_dll="/usr/x86_64-w64-mingw32/lib/qt6/plugins/imageformats/qsvg.dll"
    if [ -f "$svg_dll" ]; then cp -n "$svg_dll" "$target_dir/imageformats/"; fi

    log_success "Windows DLLs collected in $target_dir"
}

# --- Command Implementation ---
case "$1" in
    setup)
        log_info "Starting automated dependency setup..."
        ensure_sudo
        if [ -f /etc/debian_version ]; then
            sudo apt-get update
            PKGS=(build-essential cmake ninja-build python3 python3-pillow qt6-base-dev qt6-svg-dev qt6-opengl-dev libgl1-mesa-dev mingw-w64 g++-mingw-w64 patchelf)
            sudo apt-get install -y "${PKGS[@]}"
        elif [ -f /etc/arch-release ]; then
            if ! grep -q "\[ownstuff\]" /etc/pacman.conf; then
                sudo pacman-key --keyserver keyserver.ubuntu.com --recv-keys B9E36A7275FC61B464B67907E06FE8F53CDC6A4C
                sudo pacman-key --lsign-key B9E36A7275FC61B464B67907E06FE8F53CDC6A4C
                echo -e "\n[ownstuff]\nSigLevel = Optional TrustAll\nServer = https://ftp.f3l.de/~martchus/\$repo/os/\$arch" | sudo tee -a /etc/pacman.conf
                sudo pacman -Sy
            fi
            PKGS=(base-devel cmake ninja python python-pillow qt6-base qt6-svg mingw-w64-gcc mingw-w64-qt6-base mingw-w64-qt6-svg patchelf)
            sudo pacman -S --needed --noconfirm "${PKGS[@]}"
        elif [ -f /etc/fedora-release ]; then
            PKGS=(gcc-c++ cmake ninja-build python3 python3-pillow qt6-qtbase-devel qt6-qtsvg-devel mesa-libGL-devel mingw64-gcc-c++ mingw64-qt6-qtbase mingw64-qt6-qtsvg patchelf)
            sudo dnf install -y "${PKGS[@]}"
        fi
        log_success "Setup process completed."
        ;;

    gen)
        check_requirements
        log_info "Starting GenBuild (Linux Native)..."
        mkdir -p "$BUILD_DIR_LINUX"
        cd "$BUILD_DIR_LINUX"
        cmake "$SOURCE_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
        ninja
        cd "$ROOT_DIR"
        deploy_linux_libs
        ;;

    win)
        check_requirements
        log_info "Starting WinBuild (Windows Cross-compile)..."
        TOOLCHAIN_FILE="$SOURCE_DIR/cmake/x86_64-w64-mingw32.cmake"
        [ -f "$TOOLCHAIN_FILE" ] || log_error "Toolchain file missing: $TOOLCHAIN_FILE"
        mkdir -p "$BUILD_DIR_WINDOWS"
        cd "$BUILD_DIR_WINDOWS"
        cmake "$SOURCE_DIR" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" -DCMAKE_BUILD_TYPE=Release
        ninja
        cd "$ROOT_DIR"
        deploy_windows_dlls
        ;;

    test)
        if [ -d "$BUILD_DIR_LINUX" ]; then
            cd "$BUILD_DIR_LINUX" && LD_LIBRARY_PATH=. ctest --output-on-failure
        else
            log_error "Build directory not found. Run './manage.sh gen' first."
        fi
        ;;

    run)
        EXE="$BUILD_DIR_LINUX/HXPainter"
        if [ -f "$EXE" ]; then
            log_info "Launching HXPainter (Linux) with local libraries..."
            cd "$BUILD_DIR_LINUX" && LD_LIBRARY_PATH=. ./HXPainter "$@"
        else
            log_error "Executable not found. Run './manage.sh gen' first."
        fi
        ;;

    clean)
        log_info "Cleaning build artifacts..."
        rm -rf "$BUILD_DIR_LINUX" "$BUILD_DIR_WINDOWS"
        log_success "Build directories and all collected libraries removed."
        ;;

    *)
        show_help
        ;;
esac
