/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pkg.h"

#ifndef PKGCONFU_DEFAULT_PATH
#define PKGCONFU_DEFAULT_PATH \
	"/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig:" \
	"/usr/lib/pkgconfig:/usr/share/pkgconfig"
#endif

void pkg_ctx_init(pkg_ctx *ctx)
{
	strlist_init(&ctx->path);
	strlist_init(&ctx->defines);
	ctx->loaded = nullptr;
	ctx->nloaded = 0;
	ctx->loadcap = 0;
	strbuf_init(&ctx->errors);
}

void pkg_ctx_free(pkg_ctx *ctx)
{
	strlist_free(&ctx->path);
	strlist_free(&ctx->defines);
	for (size_t i = 0; i < ctx->nloaded; i++)
		package_free(ctx->loaded[i]);
	free(ctx->loaded);
	strbuf_free(&ctx->errors);
}

void pkg_add_path(pkg_ctx *ctx, const char *dir)
{
	if (*dir && !strlist_contains(&ctx->path, dir))
		strlist_push(&ctx->path, dir);
}

void pkg_add_path_list(pkg_ctx *ctx, const char *list)
{
	const char *s = list;
	while (*s) {
		const char *sep = strchr(s, ':');
		size_t n = sep ? (size_t)(sep - s) : strlen(s);
		if (n) {
			char *dir = xstrndup(s, n);
			pkg_add_path(ctx, dir);
			free(dir);
		}
		if (!sep)
			break;
		s = sep + 1;
	}
}

void pkg_default_paths(pkg_ctx *ctx)
{
	const char *libdir = getenv("PKG_CONFIG_LIBDIR");
	pkg_add_path_list(ctx, libdir && *libdir ? libdir : PKGCONFU_DEFAULT_PATH);
}

static package *cached(pkg_ctx *ctx, const char *name)
{
	for (size_t i = 0; i < ctx->nloaded; i++)
		if (strcmp(ctx->loaded[i]->key, name) == 0)
			return ctx->loaded[i];
	return nullptr;
}

static void cache_add(pkg_ctx *ctx, package *p)
{
	if (ctx->nloaded == ctx->loadcap) {
		ctx->loadcap = ctx->loadcap ? ctx->loadcap * 2 : 8;
		ctx->loaded = xrealloc(ctx->loaded,
				       ctx->loadcap * sizeof(*ctx->loaded));
	}
	ctx->loaded[ctx->nloaded++] = p;
}

package *pkg_load(pkg_ctx *ctx, const char *name)
{
	package *c = cached(ctx, name);
	if (c)
		return c;

	for (size_t i = 0; i < ctx->path.len; i++) {
		char *file = xasprintf("%s/%s.pc", ctx->path.items[i], name);
		if (access(file, R_OK) == 0) {
			package *p = package_parse_file(file, name,
							&ctx->defines,
							&ctx->errors);
			free(file);
			if (p) {
				cache_add(ctx, p);
				return p;
			}
			return nullptr;
		}
		free(file);
	}
	return nullptr;
}

static bool risalnum(char c)
{
	return isalnum((unsigned char)c) != 0;
}

int pkg_vercmp(const char *a, const char *b)
{
	if (strcmp(a, b) == 0)
		return 0;

	const char *one = a;
	const char *two = b;

	while (*one || *two) {
		while (*one && !risalnum(*one) && *one != '~')
			one++;
		while (*two && !risalnum(*two) && *two != '~')
			two++;

		if (*one == '~' || *two == '~') {
			if (*one != '~')
				return 1;
			if (*two != '~')
				return -1;
			one++;
			two++;
			continue;
		}

		if (!*one || !*two)
			break;

		const char *s1 = one;
		const char *s2 = two;
		bool isnum;
		if (isdigit((unsigned char)*s1)) {
			while (isdigit((unsigned char)*one))
				one++;
			while (isdigit((unsigned char)*two))
				two++;
			isnum = true;
		} else {
			while (isalpha((unsigned char)*one))
				one++;
			while (isalpha((unsigned char)*two))
				two++;
			isnum = false;
		}

		if (two == s2)
			return isnum ? 1 : -1;

		if (isnum) {
			while (*s1 == '0')
				s1++;
			while (*s2 == '0')
				s2++;
			size_t n1 = (size_t)(one - s1);
			size_t n2 = (size_t)(two - s2);
			if (n1 > n2)
				return 1;
			if (n2 > n1)
				return -1;
		}

		size_t l1 = (size_t)(one - s1);
		size_t l2 = (size_t)(two - s2);
		size_t ml = l1 < l2 ? l1 : l2;
		int rc = strncmp(s1, s2, ml);
		if (rc)
			return rc < 0 ? -1 : 1;
		if (l1 > l2)
			return 1;
		if (l1 < l2)
			return -1;
	}

	if (!*one && !*two)
		return 0;
	return *one ? 1 : -1;
}

bool pkg_op_satisfied(cmp_op op, const char *have, const char *want)
{
	int c = pkg_vercmp(have ? have : "", want ? want : "");
	switch (op) {
	case CMP_ANY:
		return true;
	case CMP_LT:
		return c < 0;
	case CMP_LE:
		return c <= 0;
	case CMP_EQ:
		return c == 0;
	case CMP_NE:
		return c != 0;
	case CMP_GE:
		return c >= 0;
	case CMP_GT:
		return c > 0;
	}
	return false;
}

const char *pkg_op_name(cmp_op op)
{
	switch (op) {
	case CMP_LT:
		return "<";
	case CMP_LE:
		return "<=";
	case CMP_EQ:
		return "=";
	case CMP_NE:
		return "!=";
	case CMP_GE:
		return ">=";
	case CMP_GT:
		return ">";
	default:
		return "";
	}
}

static bool is_op_char(char c)
{
	return c == '<' || c == '>' || c == '=' || c == '!';
}

static cmp_op parse_op(const char *t)
{
	if (strcmp(t, "<") == 0)
		return CMP_LT;
	if (strcmp(t, "<=") == 0)
		return CMP_LE;
	if (strcmp(t, "=") == 0 || strcmp(t, "==") == 0)
		return CMP_EQ;
	if (strcmp(t, "!=") == 0)
		return CMP_NE;
	if (strcmp(t, ">=") == 0)
		return CMP_GE;
	if (strcmp(t, ">") == 0)
		return CMP_GT;
	return CMP_ANY;
}

size_t pkg_parse_deps(const char *s, pkg_dep **out)
{
	*out = nullptr;
	if (!s)
		return 0;

	strlist tok;
	strlist_init(&tok);
	strbuf b;
	strbuf_init(&b);

	for (const char *p = s; *p;) {
		if (isspace((unsigned char)*p) || *p == ',') {
			p++;
			continue;
		}
		strbuf_reset(&b);
		if (is_op_char(*p)) {
			while (is_op_char(*p))
				strbuf_addc(&b, *p++);
		} else {
			while (*p && !isspace((unsigned char)*p) && *p != ',' &&
			       !is_op_char(*p))
				strbuf_addc(&b, *p++);
		}
		strlist_push(&tok, b.data);
	}
	strbuf_free(&b);

	pkg_dep *deps = nullptr;
	size_t n = 0;
	size_t i = 0;
	while (i < tok.len) {
		if (is_op_char(tok.items[i][0])) {
			i++;
			continue;
		}
		pkg_dep d = { .name = xstrdup(tok.items[i++]),
			      .op = CMP_ANY,
			      .version = nullptr };
		if (i < tok.len && is_op_char(tok.items[i][0])) {
			d.op = parse_op(tok.items[i++]);
			if (i < tok.len)
				d.version = xstrdup(tok.items[i++]);
			else
				d.version = xstrdup("");
		}
		deps = xrealloc(deps, (n + 1) * sizeof(*deps));
		deps[n++] = d;
	}

	strlist_free(&tok);
	*out = deps;
	return n;
}

void pkg_deps_free(pkg_dep *d, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		free(d[i].name);
		free(d[i].version);
	}
	free(d);
}

void pkglist_free(pkglist *l)
{
	free(l->items);
	free(l->depth);
	l->items = nullptr;
	l->depth = nullptr;
	l->len = l->cap = 0;
}

static long pkglist_index(const pkglist *l, const package *p)
{
	for (size_t i = 0; i < l->len; i++)
		if (l->items[i] == p)
			return (long)i;
	return -1;
}

static void pkglist_push(pkglist *l, package *p, size_t depth)
{
	if (l->len == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 8;
		l->items = xrealloc(l->items, l->cap * sizeof(*l->items));
		l->depth = xrealloc(l->depth, l->cap * sizeof(*l->depth));
	}
	l->items[l->len] = p;
	l->depth[l->len] = depth;
	l->len++;
}

static int add_pkg(pkg_ctx *ctx, const pkg_dep *dep, bool want_private,
		   size_t depth, pkglist *out, strlist *stack)
{
	package *p = pkg_load(ctx, dep->name);
	if (!p) {
		strbuf_addf(&ctx->errors,
			    "Package '%s' was not found in the pkg-config search path\n",
			    dep->name);
		return -1;
	}
	if (dep->op != CMP_ANY &&
	    !pkg_op_satisfied(dep->op, p->version, dep->version)) {
		strbuf_addf(&ctx->errors,
			    "Requested '%s %s %s' but version of %s is %s\n",
			    dep->name, pkg_op_name(dep->op),
			    dep->version ? dep->version : "",
			    p->name ? p->name : dep->name,
			    p->version ? p->version : "");
		return -1;
	}

	if (strlist_contains(stack, p->key))
		return 0;

	long idx = pkglist_index(out, p);
	if (idx >= 0) {
		if (depth <= out->depth[idx])
			return 0;
		out->depth[idx] = depth;
	} else {
		pkglist_push(out, p, depth);
	}

	strlist_push(stack, p->key);
	int rc = 0;
	pkg_dep *sub;
	size_t nsub = pkg_parse_deps(p->requires_str, &sub);
	for (size_t i = 0; i < nsub; i++)
		if (add_pkg(ctx, &sub[i], want_private, depth + 1, out,
			    stack) != 0)
			rc = -1;
	pkg_deps_free(sub, nsub);

	if (want_private) {
		nsub = pkg_parse_deps(p->requires_private_str, &sub);
		for (size_t i = 0; i < nsub; i++)
			if (add_pkg(ctx, &sub[i], want_private, depth + 1, out,
				    stack) != 0)
				rc = -1;
		pkg_deps_free(sub, nsub);
	}
	strlist_remove_at(stack, stack->len - 1);
	return rc;
}

int pkg_closure(pkg_ctx *ctx, const pkg_dep *roots, size_t nroots,
		bool want_private, pkglist *out)
{
	int rc = 0;
	strlist stack;
	strlist_init(&stack);
	for (size_t i = 0; i < nroots; i++)
		if (add_pkg(ctx, &roots[i], want_private, 0, out, &stack) != 0)
			rc = -1;
	strlist_free(&stack);

	for (size_t i = 1; i < out->len; i++) {
		package *pi = out->items[i];
		size_t di = out->depth[i];
		size_t j = i;
		while (j > 0 && out->depth[j - 1] > di) {
			out->items[j] = out->items[j - 1];
			out->depth[j] = out->depth[j - 1];
			j--;
		}
		out->items[j] = pi;
		out->depth[j] = di;
	}
	return rc;
}
