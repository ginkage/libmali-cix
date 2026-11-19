/* SPDX-License-Identifier: MIT */
/*
 * Minimal reader for app.json. It only needs to answer "is this name in that list", so it is a
 * single-pass recursive-descent parser over the (comment-stripped) text that records, for the
 * top-level keys it knows, whether the application name appears in the corresponding array.
 * It allocates nothing but the file buffer.
 */
#include "app_config.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DEPTH 32

struct parser {
	const char *p;
	const char *end;
	const char *app;
	bool error;
};

enum list { LIST_NONE, LIST_MALI, LIST_MESA, LIST_SWRAST };

struct result {
	bool in_mali, in_mesa, in_swrast;
	bool swrast_enabled; /* "swrast": {"enable": false} turns the list off; arrays are enabled */
};

static void skip_ws(struct parser *ps)
{
	while (ps->p < ps->end) {
		char c = *ps->p;

		if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == ':') {
			/* ',' and ':' are treated as whitespace: the structure is implied by the grammar
			 * below, and this keeps trailing commas (common in hand-edited files) harmless. */
			ps->p++;
		} else if (c == '/' && ps->p + 1 < ps->end && ps->p[1] == '/') {
			while (ps->p < ps->end && *ps->p != '\n')
				ps->p++;
		} else if (c == '/' && ps->p + 1 < ps->end && ps->p[1] == '*') {
			const char *close = NULL;

			for (const char *q = ps->p + 2; q + 1 < ps->end; q++) {
				if (q[0] == '*' && q[1] == '/') {
					close = q;
					break;
				}
			}
			if (!close) {
				ps->error = true;
				ps->p = ps->end;
				return;
			}
			ps->p = close + 2;
		} else {
			return;
		}
	}
}

/* Parses a string at ps->p (which must be '"'); returns whether it equals @cmp (if non-NULL). */
static bool parse_string(struct parser *ps, const char *cmp)
{
	const char *start = ++ps->p;
	bool escaped = false;

	while (ps->p < ps->end && *ps->p != '"') {
		if (*ps->p == '\\') {
			escaped = true;
			ps->p++;
		}
		ps->p++;
	}
	if (ps->p >= ps->end) {
		ps->error = true;
		return false;
	}
	size_t len = (size_t)(ps->p - start);

	ps->p++; /* closing quote */
	/* Process names never contain escapes; compare raw bytes and never match escaped strings. */
	return cmp && !escaped && strlen(cmp) == len && memcmp(start, cmp, len) == 0;
}

static void parse_value(struct parser *ps, int depth, enum list list, struct result *res);

static void parse_array(struct parser *ps, int depth, enum list list, struct result *res)
{
	ps->p++; /* '[' */
	for (;;) {
		skip_ws(ps);
		if (ps->error || ps->p >= ps->end) {
			ps->error = true;
			return;
		}
		if (*ps->p == ']') {
			ps->p++;
			return;
		}
		if (*ps->p == '"' && list != LIST_NONE) {
			if (parse_string(ps, ps->app)) {
				if (list == LIST_MALI)
					res->in_mali = true;
				else if (list == LIST_MESA)
					res->in_mesa = true;
				else if (list == LIST_SWRAST)
					res->in_swrast = true;
			}
		} else {
			parse_value(ps, depth + 1, LIST_NONE, res);
		}
	}
}

static enum list key_list(const char *start, size_t len, int depth, enum list parent)
{
#define KEY(s) (len == sizeof(s) - 1 && memcmp(start, s, len) == 0)
	if (depth == 0) {
		if (KEY("cix") || KEY("mali"))
			return LIST_MALI;
		if (KEY("mesa"))
			return LIST_MESA;
		if (KEY("swrast"))
			return LIST_SWRAST;
	} else if (depth == 1 && parent == LIST_SWRAST && KEY("apps")) {
		return LIST_SWRAST;
	}
	return LIST_NONE;
#undef KEY
}

static void parse_object(struct parser *ps, int depth, enum list self, struct result *res)
{
	ps->p++; /* '{' */
	for (;;) {
		skip_ws(ps);
		if (ps->error || ps->p >= ps->end) {
			ps->error = true;
			return;
		}
		if (*ps->p == '}') {
			ps->p++;
			return;
		}
		if (*ps->p != '"') {
			ps->error = true;
			return;
		}
		const char *kstart = ps->p + 1;

		parse_string(ps, NULL);
		if (ps->error)
			return;
		size_t klen = (size_t)(ps->p - 1 - kstart);
		enum list child = key_list(kstart, klen, depth, self);

		skip_ws(ps);
		if (ps->p >= ps->end) {
			ps->error = true;
			return;
		}
		/* "swrast": {"enable": false, ...} */
		if (depth == 1 && self == LIST_SWRAST && klen == 6 && memcmp(kstart, "enable", 6) == 0) {
			if (ps->end - ps->p >= 5 && memcmp(ps->p, "false", 5) == 0)
				res->swrast_enabled = false;
		}
		parse_value(ps, depth + 1, child, res);
	}
}

static void parse_value(struct parser *ps, int depth, enum list list, struct result *res)
{
	skip_ws(ps);
	if (ps->error || ps->p >= ps->end || depth > MAX_DEPTH) {
		ps->error = true;
		return;
	}
	switch (*ps->p) {
	case '{':
		/* Objects are only meaningful at the top level and for "swrast"; their depth for key
		 * lookup is the nesting level of the object itself. */
		parse_object(ps, depth, list, res);
		break;
	case '[':
		parse_array(ps, depth, list, res);
		break;
	case '"':
		parse_string(ps, NULL);
		break;
	default: {
		/* number, true, false, null: skip the bare token */
		const char *start = ps->p;

		while (ps->p < ps->end && strchr(" \t\r\n,:[]{}\"", *ps->p) == NULL)
			ps->p++;
		if (ps->p == start) /* e.g. a stray ']' or '}' where a value belongs */
			ps->error = true;
		break;
	}
	}
}

enum mali_route mali_app_route_str(const char *json, const char *app)
{
	struct parser ps = { .p = json, .end = json + strlen(json), .app = app };
	struct result res = { .swrast_enabled = true };

	if (!app || !*app)
		return MALI_ROUTE_DEFAULT;

	skip_ws(&ps);
	if (ps.p >= ps.end || *ps.p != '{')
		return MALI_ROUTE_DEFAULT;
	parse_object(&ps, 0, LIST_NONE, &res);
	if (ps.error)
		return MALI_ROUTE_DEFAULT;

	if (res.in_mali)
		return MALI_ROUTE_MALI;
	if (res.in_swrast && res.swrast_enabled)
		return MALI_ROUTE_SWRAST;
	if (res.in_mesa)
		return MALI_ROUTE_MESA;
	return MALI_ROUTE_DEFAULT;
}

enum mali_route mali_app_route(const char *path, const char *app)
{
	FILE *f = fopen(path, "re");
	char *buf = NULL;
	size_t len = 0, cap = 0;
	enum mali_route route;

	if (!f)
		return MALI_ROUTE_DEFAULT;
	for (;;) {
		if (len + 4096 + 1 > cap) {
			if (cap >= (1u << 20)) { /* 1 MiB is plenty for a list of names */
				free(buf);
				fclose(f);
				return MALI_ROUTE_DEFAULT;
			}
			cap = cap ? cap * 2 : 8192;
			char *nbuf = realloc(buf, cap);

			if (!nbuf) {
				free(buf);
				fclose(f);
				return MALI_ROUTE_DEFAULT;
			}
			buf = nbuf;
		}
		size_t n = fread(buf + len, 1, cap - len - 1, f);

		len += n;
		if (n == 0)
			break;
	}
	fclose(f);
	if (!buf)
		return MALI_ROUTE_DEFAULT;
	buf[len] = '\0';
	route = mali_app_route_str(buf, app);
	free(buf);
	return route;
}

const char *mali_route_name(enum mali_route route)
{
	switch (route) {
	case MALI_ROUTE_MALI:
		return "mali";
	case MALI_ROUTE_MESA:
		return "mesa";
	case MALI_ROUTE_SWRAST:
		return "swrast";
	default:
		return "default (mali)";
	}
}
