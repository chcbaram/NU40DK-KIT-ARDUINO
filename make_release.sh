#!/bin/bash

# 에러 발생 시 스크립트 중단
set -e

# 입력 파라미터 확인
if [ "$#" -ne 2 ]; then
    echo "사용법: $0 <대상_폴더_경로> <버전>"
    echo "예시: $0 ./my_project 0.0.1"
    exit 1
fi

TARGET_DIR=$1
VERSION=$2

# 대상 폴더가 존재하는지 확인
if [ ! -d "$TARGET_DIR" ]; then
    echo "에러: '$TARGET_DIR' 폴더가 존재하지 않습니다."
    exit 1
fi

# 고정된 접두사를 사용하여 압축 파일명 생성
ARCHIVE_NAME="baram-nu40dk-kit-${VERSION}.tar.bz2"

echo "==> '${TARGET_DIR}' 폴더를 '${ARCHIVE_NAME}'으로 압축 중..."

# 대상 폴더의 상위 디렉토리와 폴더명 추출
PARENT_DIR=$(dirname "$TARGET_DIR")
DIR_NAME=$(basename "$TARGET_DIR")

# 1. tar.bz2 압축 파일 생성
tar -cjf "$ARCHIVE_NAME" -C "$PARENT_DIR" "$DIR_NAME"

echo "==> 압축 완료. 정보 추출 중..."

# 2. 파일 크기(Size) 추출
FILE_SIZE=$(stat -f%z "$ARCHIVE_NAME")

# 3. SHA-256 체크섬 생성 및 대문자 변환
SHA256_HASH=$(shasum -a 256 "$ARCHIVE_NAME" | awk '{print $1}' | tr '[:lower:]' '[:upper:]')

# 4. 결과 출력
echo "--------------------------------------------------"
echo "결과를 복사하여 사용하세요:"
echo "--------------------------------------------------"
cat <<EOF
"version": "${VERSION}",
"category": "NUCODE_NRF52",
"url": "https://chcbaram.github.io/nu40dk-kit-board-index/boards/${ARCHIVE_NAME}",
"archiveFileName": "${ARCHIVE_NAME}",
"checksum": "SHA-256:${SHA256_HASH}",
"size": "${FILE_SIZE}",
EOF
echo "--------------------------------------------------"