/* SPDX-License-Identifier: MIT */
#include "app_config.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(const char *json, const char *app, enum mali_route want, const char *what)
{
	enum mali_route got = mali_app_route_str(json, app);

	if (got != want) {
		printf("FAIL %s: %s -> %s, want %s\n", what, app, mali_route_name(got), mali_route_name(want));
		failures++;
	}
}

static const char cix_style[] =
	"{\n"
	"    \"file_format_version\" : \"0.0.1\",\n"
	"    \"cix\" : [\n"
	"        // compositors\n"
	"        \"gnome-shell\",\n"
	"        \"Xwayland\",\n"
	"    ],\n"
	"    \"mesa\" : [ \"glmark2-wayland\", \"plasmashell\", \"gnome-shell-x\" ],\n"
	"    /* block\n comment */\n"
	"    \"swrast\": { \"enable\": true, \"apps\": [ \"code\" ] },\n"
	"    \"context_priority\": { \"enable\": true, \"apps\": { \"glmark2-wayland\": 0, \"mesa\": 1 } },\n"
	"    \"log\": { \"enable\": false, \"level\" : 2 },\n"
	"    \"dump_shader\": { \"enable\": false, \"path\": \"/tmp\" }\n"
	"}\n";

int main(void)
{
	expect(cix_style, "gnome-shell", MALI_ROUTE_MALI, "cix list");
	expect(cix_style, "Xwayland", MALI_ROUTE_MALI, "cix list after comment");
	expect(cix_style, "glmark2-wayland", MALI_ROUTE_MESA, "mesa list");
	expect(cix_style, "plasmashell", MALI_ROUTE_MESA, "mesa list middle");
	expect(cix_style, "code", MALI_ROUTE_SWRAST, "swrast apps");
	expect(cix_style, "glmark2-es2-wayland", MALI_ROUTE_DEFAULT, "unlisted");
	expect(cix_style, "gnome", MALI_ROUTE_DEFAULT, "prefix is not a match");
	expect(cix_style, "mesa", MALI_ROUTE_DEFAULT, "key names elsewhere are not entries");
	expect(cix_style, "enable", MALI_ROUTE_DEFAULT, "nested keys are not entries");
	expect(cix_style, "/tmp", MALI_ROUTE_DEFAULT, "nested values are not entries");
	expect(cix_style, "", MALI_ROUTE_DEFAULT, "empty name");

	expect("{ \"swrast\": { \"enable\": false, \"apps\": [\"code\"] }, \"mesa\": [\"code\"] }", "code",
	       MALI_ROUTE_MESA, "disabled swrast falls back to mesa");
	expect("{ \"swrast\": [\"code\"] }", "code", MALI_ROUTE_SWRAST, "swrast as plain array");
	expect("{ \"mali\": [\"a\"], \"mesa\": [\"a\"] }", "a", MALI_ROUTE_MALI, "mali alias wins over mesa");
	expect("{ \"mesa\": [\"a\\\"b\"] }", "a\"b", MALI_ROUTE_DEFAULT, "escaped strings never match");

	/* malformed input routes to the default and terminates */
	expect("{ \"mesa\": [\"a\" ", "a", MALI_ROUTE_DEFAULT, "unterminated array");
	expect("{ \"mesa\": [\"a\"] /* open", "a", MALI_ROUTE_DEFAULT, "unterminated comment");
	expect("{ \"mesa\": ] }", "a", MALI_ROUTE_DEFAULT, "stray bracket");
	expect("{ \"mesa\": [\"a\", } ] }", "a", MALI_ROUTE_DEFAULT, "stray brace in array");
	expect("[\"a\"]", "a", MALI_ROUTE_DEFAULT, "top level must be an object");
	expect("", "a", MALI_ROUTE_DEFAULT, "empty document");
	expect("{\"mesa\": [\"a\"", "a", MALI_ROUTE_DEFAULT, "truncated");

	if (mali_app_route("/nonexistent/app.json", "a") != MALI_ROUTE_DEFAULT) {
		printf("FAIL missing file\n");
		failures++;
	}

	if (failures)
		return 1;
	printf("all app_config tests passed\n");
	return 0;
}
