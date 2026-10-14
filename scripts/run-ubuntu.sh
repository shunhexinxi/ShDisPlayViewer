#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export XDG_CACHE_HOME="$PWD/build/ubuntu-release/cache"
mkdir -p "$XDG_CACHE_HOME/fontconfig"
SHEARTH_SDK_ROOT="${SHEARTH_SDK_ROOT:-$PWD/sdk/ShEarthSDK/ubuntu2204-x64}"
SHVIDEO_SDK_ROOT="${SHVIDEO_SDK_ROOT:-$PWD/sdk/ShVideoSDK/ubuntu2204-x64}"
export LD_LIBRARY_PATH="${QT_ROOT:-/nonexistent}/lib:$SHEARTH_SDK_ROOT/lib:$SHEARTH_SDK_ROOT/lib/osgPlugins-3.6.5:$SHVIDEO_SDK_ROOT/lib:$PWD/build/ubuntu-release/private/ShVideoSDK:${LD_LIBRARY_PATH:-}"
export OSG_LIBRARY_PATH="$SHEARTH_SDK_ROOT/lib/osgPlugins-3.6.5:${OSG_LIBRARY_PATH:-}"
exec ./build/ubuntu-release/ShDisplayViewer "$@"
