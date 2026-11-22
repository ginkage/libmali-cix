# Vulkan WSI layer

`libVkLayer_window_system_integration.so` and its manifest come from the CIX variant of
[ginkage/vulkan-wsi-layer](https://github.com/ginkage/vulkan-wsi-layer), built by its CI with

    -DWSIALLOC_MEMORY_HEAP_NAME=system -DWSI_REPORT_LAYERED_DRIVER_DEFAULT=OFF -DWSI_DMABUF_HEAP_COHERENT=ON

The release is pinned in `scripts/update_wsi.sh` (`WSI_DEB_URL`, checked against `WSI_DEB_SHA256`):
currently `v1.3.276-1-cb898f8`, package `libvulkan-wsi-layer_1.3.276-1+cix1_arm64.deb`.
CI runs that script before building, so a release always ships the pinned layer; the copy here is the
same, so that local builds match. To move to a newer layer, update the pin, run the script and commit.
It also takes a local CMake build dir (`cix-go/tools/build-wsi-layer.sh`) or another `.deb` for testing.
The manifest's `library_path` is rewritten to the bare soname (the layer lives in <libdir>/mali, see
mali-switch).
