#!/bin/bash
# Shared by the macOS entry points; source this file, do not execute it.
CG_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -z "${CG_VULKAN_ENV:-}" ]]; then
    if [[ -r "$CG_PROJECT_ROOT/../vulkan-env.sh" ]]; then
        CG_VULKAN_ENV="$CG_PROJECT_ROOT/../vulkan-env.sh"
    else
        CG_VULKAN_ENV="$HOME/VulkanSDK/1.4.321.0/setup-env.sh"
    fi
fi
if [[ ! -r "$CG_VULKAN_ENV" ]]; then
    printf 'Cannot read Vulkan environment script: %s\nInstall LunarG SDK 1.4.321.0 or set CG_VULKAN_ENV to setup-env.sh.\n' "$CG_VULKAN_ENV" >&2
    return 1
fi
# LunarG prints a notice on stdout; keep CMake's machine-readable output clean.
source "$CG_VULKAN_ENV" >/dev/null || return 1
export PATH="$VULKAN_SDK/bin:/usr/local/bin:$PATH"
if [[ ! -x "$VULKAN_SDK/bin/glslc" || ! -r "$VK_DRIVER_FILES" ]]; then
    printf 'The Vulkan SDK tools or MoltenVK manifest are missing.\n' >&2
    return 1
fi

# Only SDK variables are written; no credentials or full shell environment.
# VS Code's C++ debugger loads this ignored, local file after the build task.
mkdir -p "$CG_PROJECT_ROOT/build-debug"
{
    printf 'VULKAN_SDK=%s\n' "$VULKAN_SDK"
    printf 'VK_DRIVER_FILES=%s\n' "$VK_DRIVER_FILES"
    printf 'VK_ICD_FILENAMES=%s\n' "$VK_ICD_FILENAMES"
    printf 'VK_ADD_LAYER_PATH=%s\n' "$VK_ADD_LAYER_PATH"
    printf 'DYLD_LIBRARY_PATH=%s\n' "$DYLD_LIBRARY_PATH"
} > "$CG_PROJECT_ROOT/build-debug/macos-debug.env"
