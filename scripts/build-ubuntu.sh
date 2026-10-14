#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${QT_ROOT:?Set QT_ROOT to Qt 6.11.x gcc_64}"
# PACKAGE mode needs runtime libraries; only private SOURCE builds require FFmpeg development metadata.
SHEARTH_SDK_ROOT="${SHEARTH_SDK_ROOT:-$PWD/sdk/ShEarthSDK/ubuntu2204-x64}"
SHVIDEO_SDK_ROOT="${SHVIDEO_SDK_ROOT:-$PWD/sdk/ShVideoSDK/ubuntu2204-x64}"
export LD_LIBRARY_PATH="${QT_ROOT}/lib:$SHEARTH_SDK_ROOT/lib:$SHVIDEO_SDK_ROOT/lib:${LD_LIBRARY_PATH:-}"
cmake --preset ubuntu-release "$@"
cmake --build --preset ubuntu-release --parallel "${SHDISPLAY_JOBS:-4}"
ctest --preset ubuntu-release
