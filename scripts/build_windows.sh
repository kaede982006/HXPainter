#!/bin/bash

# HXPainter Windows 빌드 스크립트
# Linux 에서 Windows용 이진물을 빌드합니다.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/../../HXPainter"
BUILD_DIR="${PROJECT_ROOT}/build-windows"

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

echo -e "${GREEN}✓ 모든 의존성이 설치되었습니다.${NC}"

# 2. 빌드 디렉토리 정리
echo -e "${YELLOW}[2/5] 빌드 디렉토리 정리...${NC}"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# 3. CMake 설정
echo -e "${YELLOW}[3/5] CMake 설정...${NC}"
cmake "${PROJECT_ROOT}" -G Ninja -DCMAKE_BUILD_TYPE=Release

# 4. 빌드
echo -e "${YELLOW}[4/5] 빌드 중...${NC}"
ninja

# 5. 압축
echo -e "${YELLOW}[5/5] 압축 중...${NC}"
OUTPUT_FILE="${SCRIPT_DIR}/HXPainter-HXPainter-Windows-$(date +%Y%m%d-%H%M%S).tar.xz"
tar -cf - -C "${BUILD_DIR}" HXPainter logo.png | xz -c > "${OUTPUT_FILE}"
echo -e "${GREEN}압축 생성: ${OUTPUT_FILE}${NC}"

# 6. 실행 권한 설정
chmod +x "${OUTPUT_FILE%.*}/HXPainter"

echo ""
echo "=========================================="
echo -e "${GREEN}빌드 완료!${NC}"
echo "=========================================="
echo "출력 파일: ${OUTPUT_FILE%.*}"
echo "크기: $(du -h ${OUTPUT_FILE%.*} | cut -f1)"
echo ""
echo "사용 방법:"
echo "  1. 압축을 해제하세요."
echo "  2. HXPainter 실행 파일을 실행하세요."
echo "=========================================="
