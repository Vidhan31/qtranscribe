#!/usr/bin/env bash
# ==============================================================================
# Helper script to build Native Arch Linux packages for QTranscribe using Docker
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

DEFAULT_VERSION=$(cat "${REPO_ROOT}/VERSION" 2>/dev/null | tr -d '[:space:]' || echo "1.0.0")
APP_VERSION="${1:-$DEFAULT_VERSION}"
APP_RELEASE="${2:-1}"
OUTPUT_DIR="${REPO_ROOT}/dist/arch"

echo "======================================================================"
echo " Building QTranscribe Arch Linux Package (.pkg.tar.zst)"
echo " Version: ${APP_VERSION}-${APP_RELEASE}"
echo " Output : ${OUTPUT_DIR}"
echo "======================================================================"

rm -rf "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"

docker buildx build \
    --file "${SCRIPT_DIR}/Dockerfile.arch" \
    --build-arg APP_VERSION="${APP_VERSION}" \
    --build-arg APP_RELEASE="${APP_RELEASE}" \
    --target export \
    --output type=local,dest="${OUTPUT_DIR}" \
    "${REPO_ROOT}"

echo "======================================================================"
echo " Build successful! Arch package artifacts exported to: ${OUTPUT_DIR}"
ls -lh "${OUTPUT_DIR}"
echo "======================================================================"
