/* SPDX-License-Identifier: MIT */
/*
 * libEGL_mali: a glvnd EGL vendor library that routes each process either to libmali or to the next
 * vendor (Mesa), per /etc/mali/app.json.
 *
 * glvnd loads every vendor listed in egl_vendor.d and calls its __egl_Main() once per process; a
 * vendor that returns EGL_FALSE is skipped. So this library either forwards that call to libmali's
 * own glvnd entry point (eglp_Main, which libEGL_cix forwards to unchanged as well), or declines
 * and lets glvnd fall through to Mesa:
 *
 *   - no /dev/mali0 (Panthor mode)       -> decline
 *   - app listed under "mesa"           -> MESA_LOADER_DRIVER_OVERRIDE=zink, decline
 *   - app listed under "swrast"         -> LIBGL_ALWAYS_SOFTWARE=1, decline
 *   - otherwise ("cix"/"mali" or unlisted) -> libmali
 *
 * Environment: MALI_EGL={mali|mesa|swrast|off} overrides the decision (and the /dev/mali0 check);
 * MALI_EGL_CONFIG names another app.json; MALI_EGL_DEBUG=1 logs the decision to stderr. Variables
 * this library sets are never overwritten if the user already set them.
 *
 * The glvnd vendor ABI arguments are passed through opaquely, so no glvnd headers are needed.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app_config.h"

#ifndef MALI_APP_CONFIG
#define MALI_APP_CONFIG "/etc/mali/app.json"
#endif
#ifndef MALI_LIBRARY
#define MALI_LIBRARY "libmali.so.0"
#endif
#ifndef MALI_DEVICE
#define MALI_DEVICE "/dev/mali0"
#endif

#define EXPORT __attribute__((visibility("default")))

typedef unsigned int egl_boolean; /* EGLBoolean */
typedef egl_boolean (*egl_main_fn)(uint32_t version, const void *exports, void *vendor, void *imports);

static bool debug_enabled(void)
{
	const char *v = getenv("MALI_EGL_DEBUG");

	return v && *v && strcmp(v, "0") != 0;
}

static void __attribute__((format(printf, 1, 2))) debug(const char *fmt, ...)
{
	va_list ap;

	if (!debug_enabled())
		return;
	fprintf(stderr, "libEGL_mali: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static bool route_from_env(enum mali_route *route, bool *off)
{
	const char *v = getenv("MALI_EGL");

	*off = false;
	if (!v || !*v)
		return false;
	if (!strcmp(v, "mali") || !strcmp(v, "1")) {
		*route = MALI_ROUTE_MALI;
	} else if (!strcmp(v, "mesa") || !strcmp(v, "zink")) {
		*route = MALI_ROUTE_MESA;
	} else if (!strcmp(v, "swrast")) {
		*route = MALI_ROUTE_SWRAST;
	} else if (!strcmp(v, "off") || !strcmp(v, "0")) {
		*off = true;
	} else {
		debug("ignoring unknown MALI_EGL=%s", v);
		return false;
	}
	return true;
}

EXPORT egl_boolean __egl_Main(uint32_t version, const void *exports, void *vendor, void *imports)
{
	const char *app = program_invocation_short_name;
	enum mali_route route;
	bool off;

	if (route_from_env(&route, &off)) {
		if (off) {
			debug("%s: MALI_EGL=off, declining", app);
			return 0;
		}
		debug("%s: MALI_EGL forces %s", app, mali_route_name(route));
	} else {
		if (access(MALI_DEVICE, R_OK | W_OK) != 0) {
			debug("%s: %s unavailable (%s), declining", app, MALI_DEVICE, strerror(errno));
			return 0;
		}
		const char *config = getenv("MALI_EGL_CONFIG");

		if (!config || !*config)
			config = MALI_APP_CONFIG;
		route = mali_app_route(config, app);
		debug("%s: %s routes to %s", app, config, mali_route_name(route));
	}

	switch (route) {
	case MALI_ROUTE_MESA:
		setenv("MESA_LOADER_DRIVER_OVERRIDE", "zink", 0);
		return 0;
	case MALI_ROUTE_SWRAST:
		setenv("LIBGL_ALWAYS_SOFTWARE", "1", 0);
		return 0;
	default:
		break;
	}

	void *mali = dlopen(MALI_LIBRARY, RTLD_NOW | RTLD_LOCAL);

	if (!mali) {
		debug("%s: dlopen(%s) failed: %s, declining", app, MALI_LIBRARY, dlerror());
		return 0;
	}
	egl_main_fn eglp_main = (egl_main_fn)dlsym(mali, "eglp_Main");

	if (!eglp_main) {
		debug("%s: %s has no eglp_Main, declining", app, MALI_LIBRARY);
		dlclose(mali);
		return 0;
	}
	egl_boolean ok = eglp_main(version, exports, vendor, imports);

	debug("%s: eglp_Main(version 0x%x) returned %u", app, version, ok);
	/* The handle stays open for the life of the process, as glvnd keeps the vendor loaded. */
	return ok;
}
