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

## `mali-switch`

`scripts/mali-switch {mali|panthor|status}` switches the stack between the vendor driver and Panthor. It is stateless and only toggles files:
- the `blacklist` lines in `/etc/modprobe.d/blacklist-cix-gpu.conf`;
- the libmali directory in `/etc/ld.so.conf.d/00-cixgpu-pro.conf`;
- the Mali WSI layer manifest;
- the linlondp → zink driconf rule.

It then runs `ldconfig`; the switch takes effect after a reboot. In Panthor mode the gate stops resolving along with libmali, and it would decline anyway.

## Build and install (by hand, until packaged)

```sh
meson setup build && ninja -C build && meson test -C build

# next to libmali (here: the CIX umd deb's directory, which mali-switch puts on/off the linker path)
sudo install -m644 build/libEGL_mali.so.0.0.0 /opt/cixgpu-pro/lib/aarch64-linux-gnu/
sudo install -Dm644 data/app.json /etc/mali/app.json
sudo install -m644 data/10_mali.json /etc/glvnd/egl_vendor.d/10_mali.json
sudo ldconfig
```

`10_mali.json` replaces any `10_cix.json`, the plain forwarder with no routing. glvnd reads `/etc/glvnd/egl_vendor.d` before Mesa's `50_mesa.json`.
