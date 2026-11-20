#!/bin/sh -e
# SPDX-License-Identifier: MIT
#
# update_wsi.sh - refresh the bundled Vulkan WSI layer from the CIX variant of the vulkan-wsi-layer fork
# (github.com/ginkage/vulkan-wsi-layer), built with
#   -DWSIALLOC_MEMORY_HEAP_NAME=system -DWSI_REPORT_LAYERED_DRIVER_DEFAULT=OFF -DWSI_DMABUF_HEAP_COHERENT=ON
#
#   scripts/update_wsi.sh                 the pinned CI release below (CI runs this before building)
#   scripts/update_wsi.sh <url|file.deb>  another release package of that variant
#   scripts/update_wsi.sh <build dir>     a local CMake build (e.g. cix-go/build/vulkan-wsi-layer)
#
# To move to a newer layer, point WSI_DEB_URL at the "+cix" package of a newer release, set WSI_DEB_SHA256 to its
# sha256, run this and commit the result (wsi/README.md records what is bundled).
#
# Writes wsi/libVkLayer_window_system_integration.so and wsi/VkLayer_window_system_integration.json.
# The manifest's library_path becomes the bare soname: the layer is installed into <libdir>/mali,
# which is only on the linker path in vendor mode (mali-switch), so the loader skips it otherwise.

WSI_DEB_URL=https://github.com/ginkage/vulkan-wsi-layer/releases/download/v1.3.276-1-667e071/libvulkan-wsi-layer_1.3.276-1%2Bcix1_arm64.deb
WSI_DEB_SHA256=6a3723770bb3f357a567b6e0e1f9909390ca1a83b0e682aa5976085cbcf4d836

src=${1:-$WSI_DEB_URL}
top=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

if [ -d "$src" ]; then
	so=$src/libVkLayer_window_system_integration.so
	json=$src/VkLayer_window_system_integration.json
else
	deb=$src
	case $src in
	http://* | https://*)
		deb=$tmp/wsi.deb
		echo "Downloading $src"
		curl -fsSL --retry 3 -o "$deb" "$src"
		;;
	esac
	if [ "$src" = "$WSI_DEB_URL" ]; then
		echo "$WSI_DEB_SHA256  $deb" | sha256sum -c -
	fi
	dpkg-deb -x "$deb" "$tmp/root"
	so=$(find "$tmp/root" -name libVkLayer_window_system_integration.so | head -1)
	json=$(find "$tmp/root" -name VkLayer_window_system_integration.json | head -1)
fi
[ -f "$so" ] && [ -f "$json" ] || { echo "no layer .so/.json in $src" >&2; exit 1; }
# Only the CIX variant: the Rockchip one allocates from system-uncached, which CIX P1 doesn't have.
if grep -qa '/dev/dma_heap/system-uncached' "$so"; then
	echo "$src is the Rockchip variant (system-uncached heap), not the CIX one" >&2
	exit 1
fi

mkdir -p "$top/wsi"
install -m644 "$so" "$top/wsi/libVkLayer_window_system_integration.so"
sed 's|"library_path": *"[^"]*"|"library_path": "libVkLayer_window_system_integration.so"|' "$json" \
	> "$top/wsi/VkLayer_window_system_integration.json"
grep -q '"library_path": "libVkLayer_window_system_integration.so"' "$top/wsi/VkLayer_window_system_integration.json"
echo "updated wsi/ from $src"
