#!/bin/bash
set -eo pipefail
source "$(dirname "$0")/macos-env.sh"
cd "$CG_PROJECT_ROOT"
cmake --preset debug
cmake --build build-debug --parallel
