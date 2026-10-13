#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export XDG_CACHE_HOME="$PWD/build/ubuntu-release/cache"
mkdir -p "$XDG_CACHE_HOME/fontconfig"
FREEEARTH_SDK_ROOT="${FREEEARTH_SDK_ROOT:-$PWD/build/freeearth-sdk-install}"
export LD_LIBRARY_PATH="${QT_ROOT:-/nonexistent}/lib:$FREEEARTH_SDK_ROOT/lib:$FREEEARTH_SDK_ROOT/lib/osgPlugins-3.6.5:$PWD/build/ubuntu-release/sdk/videoSDK:${LD_LIBRARY_PATH:-}"
export OSG_LIBRARY_PATH="$FREEEARTH_SDK_ROOT/lib/osgPlugins-3.6.5:${OSG_LIBRARY_PATH:-}"
exec ./build/ubuntu-release/ShDisplayViewer "$@"
