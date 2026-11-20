/* SPDX-License-Identifier: MIT */
/*
 * libgbm.so.1 for vendor mode: libmali implements the GBM API itself (gbm_create_device(),
 * gbm_bo_*(), ...), so this library only has to be named libgbm.so.1 and pull libmali in (DT_NEEDED);
 * the dynamic linker then resolves the gbm_* symbols in libmali. It is installed into <libdir>/mali,
 * ahead of Mesa's libgbm.so.1 only while mali-switch puts that directory on the linker path.
 */
