#!/usr/bin/env bash
# ==============================================================================
# Helper script to build standalone Wayland Debian (.deb) package for QTranscribe
# Baseline: Ubuntu 24.04 (Noble) + Qt 6.11.1
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

DEFAULT_VERSION=$(cat "${REPO_ROOT}/VERSION" 2>/dev/null | tr -d '[:space:]' || echo "1.0.0")
UBUNTU_VERSION="${1:-24.04}"
APP_VERSION="${2:-$DEFAULT_VERSION}"
OUTPUT_DIR="${REPO_ROOT}/dist/deb"

echo "======================================================================"
echo " Building QTranscribe Debian Package (Ubuntu ${UBUNTU_VERSION})"
echo " Qt Version: 6.11.1 (Wayland Only)"
echo " Package Version: ${APP_VERSION}"
echo " Output : ${OUTPUT_DIR}"
echo "======================================================================"

rm -rf "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"

docker buildx build \
    --file "${SCRIPT_DIR}/Dockerfile.deb" \
    --build-arg UBUNTU_VERSION="${UBUNTU_VERSION}" \
    --build-arg APP_VERSION="${APP_VERSION}" \
    --target export \
    --output type=local,dest="${OUTPUT_DIR}" \
    "${REPO_ROOT}"

echo "======================================================================"
echo " Build successful! Debian package exported to: ${OUTPUT_DIR}"
ls -lh "${OUTPUT_DIR}"
echo "======================================================================"
