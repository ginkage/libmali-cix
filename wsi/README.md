# Vulkan WSI layer

`libVkLayer_window_system_integration.so` is built from [ginkage/vulkan-wsi-layer](https://github.com/ginkage/vulkan-wsi-layer)
commit `06f7d4d00dfb7f750b51fdfe46e78f3e3be9a645` with the CIX defaults
(`cix-go/tools/build-wsi-layer.sh`):

    -DWSIALLOC_MEMORY_HEAP_NAME=system -DWSI_REPORT_LAYERED_DRIVER_DEFAULT=OFF -DWSI_DMABUF_HEAP_COHERENT=ON

sha256 `a6db7e89f2714c2498e4d0d0032b5a4c39005e0915698f7d7bd3274682aa904c`.
Refresh with `scripts/update_wsi.sh <build dir | CI .deb>`, which also rewrites the manifest's
`library_path` to the bare soname (the layer lives in <libdir>/mali, see mali-switch).
