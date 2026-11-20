# libmali-cix: userspace glue for the CIX vendor GPU stack

Glue for running Arm's proprietary Mali driver (`libmali`, r54p1, as shipped by CIX for the Sky1 / CD8180 Mali-G720-Immortalis) next to Mesa on a stock distro. glvnd, the Vulkan loader and Mesa stay unmodified. The counterpart of `libmali-rockchip`. Debian packaging comes later; for now the pieces are installed by hand.

## `libEGL_mali`: per-application EGL routing

`libEGL_mali` is a glvnd EGL vendor library, the "gate". glvnd calls each vendor's `__egl_Main()` once per process and skips vendors that decline. The gate decides as follows:

| Condition | Result |
|---|---|
| no `/dev/mali0` (kernel runs Panthor) | declines: Mesa |
| app listed under `"mesa"` | sets `MESA_LOADER_DRIVER_OVERRIDE=zink`, declines: Mesa EGL with zink on libmali's Vulkan |
| app listed under `"swrast"` (when enabled) | sets `LIBGL_ALWAYS_SOFTWARE=1`, declines: Mesa software rendering |
| app listed under `"cix"`/`"mali"`, or unlisted | forwards to libmali's `eglp_Main`: libmali EGL/GLES |

libmali provides only GLES, so applications that need desktop GL on Wayland go under `"mesa"`. X11 EGL and GLX already reach Mesa, because libmali has no X11 platform.

The routing file is `/etc/mali/app.json`. It uses CIX's format: JSON with comments, and arrays of executable names (the basename of `argv[0]`). See `data/app.json`. Unknown keys are ignored, so CIX's own file can be used as is.

Environment variables:
- `MALI_EGL=mali|mesa|swrast|off` overrides the decision.
- `MALI_EGL_CONFIG=<file>` uses another routing file.
- `MALI_EGL_DEBUG=1` logs the decision to stderr.

Variables the gate sets are never overwritten if already set.

## Layout

Everything libmali-specific is installed into `<libdir>/mali` (`/usr/lib/aarch64-linux-gnu/mali`), and every manifest names a bare soname that exists only there:

| Installed | What |
|---|---|
| `<libdir>/mali/libmali.so.0.54.1` | libmali r54p1 (GLES, EGL, Vulkan, OpenCL, GBM), `blobs/` |
| `<libdir>/mali/libEGL_mali.so.0` | the gate, above |
| `<libdir>/mali/libgbm.so.1` | wrapper that pulls in libmali, which implements GBM (`src/gbm_wrapper.c`) |
| `<libdir>/mali/libVkLayer_window_system_integration.so` | Vulkan WSI layer, `wsi/` |
| `/usr/share/glvnd/egl_vendor.d/10_mali.json` | glvnd vendor → `libEGL_mali.so.0` |
| `/usr/share/vulkan/icd.d/mali.json`, `/usr/share/vulkan/implicit_layer.d/VkLayer_window_system_integration.json` | Vulkan ICD and layer |
| `/etc/OpenCL/vendors/mali.icd` | ocl-icd → `libmali.so.0` |
| `/etc/mali/app.json` | routing (conffile) |
| `/usr/lib/firmware/mali_csffw.bin` | CSF firmware, `blobs/` (must match kbase: r54p1) |
| `/usr/lib/udev/rules.d/60-mali-dma-heap.rules` | `/dev/dma_heap/*` for the render group and the active seat |
| `/usr/lib/modprobe.d/linlondp-render.conf` | `linlon_dp enable_render=1` (a render node for the display controller) |
| `/usr/share/libmali-cix/drirc-linlondp-zink.conf` | loader rule: zink instead of kmsro on linlondp |

## `mali-switch`

`mali-switch {mali|panthor|status}` switches between the vendor stack (`mali_kbase` from cix-gpu-dkms + libmali) and the upstream one (panthor + Mesa). The package installs everything inert; `mali-switch` owns the three files that turn the vendor stack on, which no package ships, so upgrades never change the mode:

- `/etc/ld.so.conf.d/00-aarch64-mali.conf`: `<libdir>/mali` on the linker path. Without it none of the manifests resolve, and glvnd, the Vulkan loader, ocl-icd and libgbm users get Mesa.
- `/etc/modprobe.d/blacklist-cix-gpu.conf`: blacklists `panthor` (vendor mode) or `mali_kbase`.
- `/etc/drirc`: a link to the linlondp → zink rule (only if no other `/etc/drirc` exists). In Panthor mode mutter's GBM needs kmsro and panfrost on linlondp.

It then runs `ldconfig`; the switch takes effect after a reboot. Removing the package switches to panthor first.

## Build and package

```sh
meson setup build && ninja -C build && meson test -C build   # development build
dpkg-buildpackage -b -uc -us                                 # libmali-cix_*.deb
```

The binaries come from CIX (`blobs/README.md`) and from the WSI layer fork (`wsi/README.md`, refreshed with `scripts/update_wsi.sh`). The package replaces CIX's `cix-gpu-umd`.
