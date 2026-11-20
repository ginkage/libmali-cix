# libmali-cix: userspace glue for the CIX vendor GPU stack

Glue for running Arm's proprietary Mali driver (`libmali`, r54p1, as shipped by CIX for the Sky1 / CD8180 Mali-G720-Immortalis) next to Mesa on a stock distro. glvnd and the Vulkan loader stay unmodified. The counterpart of `libmali-rockchip`.

## Install the whole stack

On Armbian for the CIX P1 boards (Ubuntu 26.04 "resolute", `edge` kernel, e.g. the Orion O6/O6N), one script installs the vendor stack from the latest GitHub releases of its four parts and switches to it:

| Package | Repository | What |
|---|---|---|
| `cix-gpu-dkms` | [ginkage/mali-dkms-cix](https://github.com/ginkage/mali-dkms-cix) | the Mali kernel driver (`mali_kbase`, DKMS) |
| `libmali-cix` | this one | libmali, the glue below, the Vulkan WSI layer, firmware |
| the installed Mesa packages, `+cix` | [ginkage/mesa-cix](https://github.com/ginkage/mesa-cix) | Armbian's Mesa plus zink on libmali (desktop GL) |
| `xwayland`, `+cixmali` | [ginkage/xserver](https://github.com/ginkage/xserver) (branch `24.1.10`) | Xwayland with glamor on libmali |

```sh
curl -fsSLO https://raw.githubusercontent.com/ginkage/libmali-cix/main/scripts/install-cix-gpu.sh
sh install-cix-gpu.sh -n   # dry run: what would be downloaded and installed
sh install-cix-gpu.sh      # install (asks for sudo and for apt's confirmation), then reboot
```

It replaces every installed package built from Mesa's source with the `+cix` build (they are version-locked together), installs the kernel headers DKMS needs if they are missing, pins the Mesa and Xwayland builds (`/etc/apt/preferences.d/cix-gpu.pref`) so distro updates don't replace them, and runs `mali-switch mali`. Rerun it to update. `sudo mali-switch panthor` and a reboot go back to the upstream stack (Panthor, Panfrost, PanVK), with the same Mesa.

Known limits: libmali has no desktop GL, so desktop-GL applications get zink (GL 4.6) on libmali's Vulkan. The display is driven in 8 bit (no 10-bit swapchains). Steam's web helper runs zink without kopper (`LIBGL_KOPPER_DISABLE`), which works but costs a GPU wait per frame.

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

The binaries come from CIX (`blobs/README.md`) and from the CIX variant of the WSI layer fork's CI release (`wsi/README.md`): `scripts/update_wsi.sh` downloads the pinned release, and CI runs it before building. The package replaces and provides CIX's `cix-gpu-umd`. CI builds the package in an `ubuntu:26.04` container and publishes it as a GitHub release for every push to `main`.
