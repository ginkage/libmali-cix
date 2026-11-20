# Prebuilt binaries

From [cixtech/cix_proprietary](https://github.com/cixtech/cix_proprietary), branch `cix_p1_k6.6_master`,
commit `6260fd1a596a6135a198f0f220e6ed70f3210532 CIX P1 2026Q3 RC2.3 RELEASE`, directory `cix_proprietary-debs/cix-gpu-umd/`:

| File here | Source in that tree | sha256 |
|---|---|---|
| `aarch64-linux-gnu/libmali.so.0.54.1` | `opt/cixgpu-pro/lib/aarch64-linux-gnu/libmali.so.0.54.1` (r54p1-11eac0: GLES, EGL, Vulkan ICD, OpenCL, GBM) | `8a58bee918a2ffd0d16838dbb5c2b3dcaeb089e54810dea01910e87595ccdd89` |
| `firmware/mali_csffw.bin` | `usr/lib/firmware/mali_csffw.bin` | `4f3043499f3a48fe2ea15cad559f26af77f5276f3348982a6a1c6f35dfe2572d` |

`aarch64-linux-gnu/libmali.so` is a link for the build (`cc.find_library()`).
Not included: CIX's `libEGL_cix` (replaced by `libEGL_mali`), `libgbm` dispatcher (replaced by a
wrapper, as it reads `/opt/cixgpu-compat`), `libOpenCL` (ocl-icd loads libmali instead) and the
debugging tools (`malisc`, `mali_clcc`, `gpu_utilization_clock_tracing`, ~77 MB).
The kernel driver (kbase) and this firmware must be from the same DDK release (r54p1): see mali-dkms-cix.
