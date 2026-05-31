#!/bin/bash

# HXPainter Windows 빌드 스크립트
# Linux 에서 Windows용 이진물을 빌드합니다.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-windows"
TOOLCHAIN_FILE="${PROJECT_ROOT}/cmake/x86_64-w64-mingw32.cmake"

echo "=========================================="
echo "HXPainter Windows 빌드 스크립트"
echo "=========================================="

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 1. 의존성 설치 확인
echo -e "${YELLOW}[1/5] 의존성 설치 확인...${NC}"
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}오류: cmake 가 설치되어 있지 않습니다.${NC}"
    echo -e "${YELLOW}설치 방법:${NC} sudo apt-get install cmake"
    exit 1
fi

if ! command -v ninja &> /dev/null; then
    echo -e "${RED}오류: ninja가 설치되어 있지 않습니다.${NC}"
    echo -e "${YELLOW}설치 방법:${NC} sudo apt-get install ninja-build"
    exit 1
fi

if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo -e "${RED}오류: x86_64-w64-mingw32-gcc 가 설치되어 있지 않습니다.${NC}"
    echo -e "${YELLOW}설치 방법:${NC} sudo apt-get install mingw-w64"
    exit 1
fi

if [ ! -f "${TOOLCHAIN_FILE}" ]; then
    echo -e "${RED}오류: Windows CMake 툴체인 파일이 없습니다: ${TOOLCHAIN_FILE}${NC}"
    exit 1
fi

echo -e "${GREEN}✓ 모든 의존성이 설치되었습니다.${NC}"

# 2. 빌드 디렉토리 정리
echo -e "${YELLOW}[2/5] 빌드 디렉토리 정리...${NC}"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# 3. CMake 설정
echo -e "${YELLOW}[3/5] CMake 설정...${NC}"
cmake "${PROJECT_ROOT}" -G Ninja -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" -DCMAKE_BUILD_TYPE=Release

# 4. 빌드
echo -e "${YELLOW}[4/5] 빌드 중...${NC}"
ninja

# 5. DLL 및 Qt 플러그인 수집
echo -e "${YELLOW}[5/5] Windows 런타임 파일 수집...${NC}"
(cd "${PROJECT_ROOT}" && ./manage.sh win)

OUTPUT_FILE="${SCRIPT_DIR}/HXPainter-Windows-$(date +%Y%m%d-%H%M%S).tar.xz"
PACKAGE_ITEMS=(HXPainter.exe logo.png)
for dll in "${BUILD_DIR}"/*.dll; do
    if [ -f "${dll}" ]; then
        PACKAGE_ITEMS+=("$(basename "${dll}")")
    fi
done
for dir in platforms imageformats iconengines styles; do
    if [ -d "${BUILD_DIR}/${dir}" ]; then
        PACKAGE_ITEMS+=("${dir}")
    fi
done
(cd "${BUILD_DIR}" && tar -cJf "${OUTPUT_FILE}" "${PACKAGE_ITEMS[@]}")
echo -e "${GREEN}압축 생성: ${OUTPUT_FILE}${NC}"

echo ""
echo "=========================================="
echo -e "${GREEN}빌드 완료!${NC}"
echo "=========================================="
echo "출력 파일: ${OUTPUT_FILE%.*}"
echo "크기: $(du -h "${OUTPUT_FILE}" | cut -f1)"
echo ""
echo "사용 방법:"
echo "  1. 압축을 해제하세요."
echo "  2. HXPainter.exe 실행 파일을 실행하세요."
echo "=========================================="
