#!/bin/bash
set -eo pipefail
source "$(dirname "$0")/macos-env.sh"
cd "$CG_PROJECT_ROOT"
if [[ ! -x build-debug/vulkan-starter-app ]]; then
    printf 'Build the application first: ./scripts/build-macos.sh\n' >&2
    exit 1
fi
exec ./build-debug/vulkan-starter-app "$@"
