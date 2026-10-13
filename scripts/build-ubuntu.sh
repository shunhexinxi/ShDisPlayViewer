#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${QT_ROOT:?Set QT_ROOT to Qt 6.11.x gcc_64}"
# FFmpeg 7 must be discoverable via PKG_CONFIG_PATH; osgEarth 3.8/OSG 3.6 via CMAKE_PREFIX_PATH.
export LD_LIBRARY_PATH="${QT_ROOT}/lib:${FREEEARTH_SDK_ROOT:-/nonexistent}/lib:${LD_LIBRARY_PATH:-}"
cmake --preset ubuntu-release "$@"
cmake --build --preset ubuntu-release --parallel "${SHDISPLAY_JOBS:-4}"
ctest --preset ubuntu-release
