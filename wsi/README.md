# Vulkan WSI layer

`libVkLayer_window_system_integration.so` is built from [ginkage/vulkan-wsi-layer](https://github.com/ginkage/vulkan-wsi-layer)
commit `77c6bb286e818308a2d8f37688ec1925a81b474f` with the CIX defaults
(`cix-go/tools/build-wsi-layer.sh`):

    -DWSIALLOC_MEMORY_HEAP_NAME=system -DWSI_REPORT_LAYERED_DRIVER_DEFAULT=OFF

sha256 `fd843899b59b94a308a704e792a9b8fccffd7ce6903e7bee7f0123bc428bd438`.
Refresh with `scripts/update_wsi.sh <build dir | CI .deb>`, which also rewrites the manifest's
`library_path` to the bare soname (the layer lives in <libdir>/mali, see mali-switch).
