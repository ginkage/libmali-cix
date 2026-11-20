#!/bin/sh -e
# SPDX-License-Identifier: MIT
#
# update_wsi.sh - refresh the bundled Vulkan WSI layer from a build of the vulkan-wsi-layer fork
# (github.com/ginkage/vulkan-wsi-layer) with the CIX defaults:
#   -DWSIALLOC_MEMORY_HEAP_NAME=system -DWSI_REPORT_LAYERED_DRIVER_DEFAULT=OFF -DWSI_DMABUF_HEAP_COHERENT=ON
#
#   scripts/update_wsi.sh <build dir>     a local CMake build (e.g. cix-go/build/vulkan-wsi-layer)
#   scripts/update_wsi.sh <file.deb>      a CI release package of that variant
#
# Writes wsi/libVkLayer_window_system_integration.so and wsi/VkLayer_window_system_integration.json.
# The manifest's library_path becomes the bare soname: the layer is installed into <libdir>/mali,
# which is only on the linker path in vendor mode (mali-switch), so the loader skips it otherwise.

src=${1:?usage: $0 <build dir | file.deb>}
top=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

if [ -d "$src" ]; then
	so=$src/libVkLayer_window_system_integration.so
	json=$src/VkLayer_window_system_integration.json
else
	dpkg-deb -x "$src" "$tmp"
	so=$(find "$tmp" -name libVkLayer_window_system_integration.so | head -1)
	json=$(find "$tmp" -name VkLayer_window_system_integration.json | head -1)
fi
[ -f "$so" ] && [ -f "$json" ] || { echo "no layer .so/.json in $src" >&2; exit 1; }

mkdir -p "$top/wsi"
install -m644 "$so" "$top/wsi/libVkLayer_window_system_integration.so"
sed 's|"library_path": *"[^"]*"|"library_path": "libVkLayer_window_system_integration.so"|' "$json" \
	> "$top/wsi/VkLayer_window_system_integration.json"
grep -q '"library_path": "libVkLayer_window_system_integration.so"' "$top/wsi/VkLayer_window_system_integration.json"
echo "updated wsi/ from $src"
