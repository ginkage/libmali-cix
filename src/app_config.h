/* SPDX-License-Identifier: MIT */
#ifndef MALI_APP_CONFIG_H
#define MALI_APP_CONFIG_H

/* Where an application's EGL goes. */
enum mali_route {
	MALI_ROUTE_DEFAULT, /* not listed: libmali */
	MALI_ROUTE_MALI,    /* listed under "cix" or "mali": libmali */
	MALI_ROUTE_MESA,    /* listed under "mesa": Mesa (zink on libmali's Vulkan) */
	MALI_ROUTE_SWRAST,  /* listed under "swrast" (if enabled): Mesa software rendering */
};

/*
 * Look up @app in the app.json at @path (CIX's format: JSON with // and block comments;
 * "cix"/"mali" and "mesa" are arrays of process names, "swrast" is {"enable": bool, "apps": [...]}
 * or a plain array; other keys are ignored). An explicit "cix"/"mali" entry wins over "swrast",
 * which wins over "mesa". A missing or malformed file routes everything to the default.
 */
enum mali_route mali_app_route(const char *path, const char *app);

/* Same, on an in-memory document (for tests). */
enum mali_route mali_app_route_str(const char *json, const char *app);

const char *mali_route_name(enum mali_route route);

#endif
