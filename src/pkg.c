/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <ctype.h>
#include <dirent.h>
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
	ctx->sysroot = nullptr;
	ctx->max_depth = 0;
	ctx->disable_uninstalled = false;
	ctx->define_prefix = false;
	ctx->prefix_var = "prefix";
	ctx->loaded = nullptr;
	ctx->nloaded = 0;
	ctx->loadcap = 0;
	ctx->provides_indexed = false;
	strlist_init(&ctx->alias_name);
	ctx->alias_pkg = nullptr;
	ctx->nalias = 0;
	ctx->aliascap = 0;
	strbuf_init(&ctx->errors);
}

void pkg_ctx_free(pkg_ctx *ctx)
{
	strlist_free(&ctx->path);
	strlist_free(&ctx->defines);
	free(ctx->sysroot);
	for (size_t i = 0; i < ctx->nloaded; i++)
		package_free(ctx->loaded[i]);
	free(ctx->loaded);
	strlist_free(&ctx->alias_name);
	free(ctx->alias_pkg);
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

static package *load_path(pkg_ctx *ctx, const char *name, const char *file)
{
	parse_opts opts = {
		.defines = &ctx->defines,
		.sysroot = ctx->sysroot,
		.define_prefix = ctx->define_prefix,
		.prefix_var = ctx->prefix_var,
	};
	package *p = package_parse_file(file, name, &opts, &ctx->errors);
	if (p)
		cache_add(ctx, p);
	return p;
}

static package *provides_lookup(pkg_ctx *ctx, const char *name);

static package *alias_get(pkg_ctx *ctx, const char *name)
{
	long i = strlist_index(&ctx->alias_name, name);
	return i >= 0 ? ctx->alias_pkg[i] : nullptr;
}

static void alias_add(pkg_ctx *ctx, const char *name, package *p)
{
	if (ctx->nalias == ctx->aliascap) {
		ctx->aliascap = ctx->aliascap ? ctx->aliascap * 2 : 8;
		ctx->alias_pkg = xrealloc(ctx->alias_pkg,
					  ctx->aliascap * sizeof(*ctx->alias_pkg));
	}
	strlist_push(&ctx->alias_name, name);
	ctx->alias_pkg[ctx->nalias++] = p;
}

#ifndef PKGCONFU_PKGCONFIG_COMPAT
#define PKGCONFU_PKGCONFIG_COMPAT "0.29.2"
#endif

static package *make_virtual(pkg_ctx *ctx, const char *name, const char *desc)
{
	package *p = xmalloc(sizeof(*p));
	*p = (package){ 0 };
	p->key = xstrdup(name);
	p->path = xstrdup("<virtual>");
	p->name = xstrdup(name);
	p->description = xstrdup(desc);
	p->version = xstrdup(PKGCONFU_PKGCONFIG_COMPAT);
	cache_add(ctx, p);
	return p;
}

package *pkg_load(pkg_ctx *ctx, const char *name)
{
	package *c = cached(ctx, name);
	if (c)
		return c;
	c = alias_get(ctx, name);
	if (c)
		return c;

	if (strcmp(name, "pkg-config") == 0 || strcmp(name, "pkgconf") == 0)
		return make_virtual(ctx, name, "pkgconfu");

	if (!ctx->disable_uninstalled) {
		for (size_t i = 0; i < ctx->path.len; i++) {
			char *file = xasprintf("%s/%s-uninstalled.pc",
					       ctx->path.items[i], name);
			bool ok = access(file, R_OK) == 0;
			package *p = ok ? load_path(ctx, name, file) : nullptr;
			free(file);
			if (ok)
				return p;
		}
	}

	for (size_t i = 0; i < ctx->path.len; i++) {
		char *file = xasprintf("%s/%s.pc", ctx->path.items[i], name);
		bool ok = access(file, R_OK) == 0;
		package *p = ok ? load_path(ctx, name, file) : nullptr;
		free(file);
		if (ok)
			return p;
	}

	return provides_lookup(ctx, name);
}

static package *provides_lookup(pkg_ctx *ctx, const char *name)
{
	if (ctx->provides_indexed)
		return nullptr;
	ctx->provides_indexed = true;

	for (size_t i = 0; i < ctx->path.len; i++) {
		DIR *d = opendir(ctx->path.items[i]);
		if (!d)
			continue;
		struct dirent *e;
		while ((e = readdir(d))) {
			const char *dot = strrchr(e->d_name, '.');
			if (!dot || strcmp(dot, ".pc") != 0)
				continue;
			char *mod = xstrndup(e->d_name,
					     (size_t)(dot - e->d_name));
			package *p = pkg_load(ctx, mod);
			free(mod);
			if (!p || !p->provides_str)
				continue;
			pkg_dep *pv;
			size_t npv = pkg_parse_deps(p->provides_str, &pv);
			bool hit = false;
			for (size_t j = 0; j < npv; j++)
				if (strcmp(pv[j].name, name) == 0)
					hit = true;
			pkg_deps_free(pv, npv);
			if (hit) {
				closedir(d);
				alias_add(ctx, name, p);
				return p;
			}
		}
		closedir(d);
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
	free(l->pub);
	l->items = nullptr;
	l->pub = nullptr;
	l->len = l->cap = 0;
}

static long pkglist_index(const pkglist *l, const package *p)
{
	for (size_t i = 0; i < l->len; i++)
		if (l->items[i] == p)
			return (long)i;
	return -1;
}

void pkglist_append(pkglist *l, package *p, bool pub)
{
	if (l->len == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 8;
		l->items = xrealloc(l->items, l->cap * sizeof(*l->items));
		l->pub = xrealloc(l->pub, l->cap * sizeof(*l->pub));
	}
	l->items[l->len] = p;
	l->pub[l->len] = pub;
	l->len++;
}

static package *pkglist_by_name(const pkglist *l, const char *name)
{
	for (size_t i = 0; i < l->len; i++)
		if (strcmp(l->items[i]->key, name) == 0)
			return l->items[i];
	return nullptr;
}

void pkg_err_not_found(pkg_ctx *ctx, const char *name, const char *parent)
{
	strbuf_addf(&ctx->errors,
		    "Package %s was not found in the pkg-config search path.\n"
		    "Perhaps you should add the directory containing `%s.pc'\n"
		    "to the PKG_CONFIG_PATH environment variable\n",
		    name, name);
	if (parent)
		strbuf_addf(&ctx->errors,
			    "Package '%s', required by '%s', not found\n", name,
			    parent);
	else
		strbuf_addf(&ctx->errors, "Package '%s' not found\n", name);
}

static int visit(pkg_ctx *ctx, const pkg_dep *dep, bool pub_path, int depth,
		 pkglist *out, strlist *seen, strlist *stack, const char *parent)
{
	package *p = pkg_load(ctx, dep->name);
	if (!p) {
		pkg_err_not_found(ctx, dep->name, parent);
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

	bool already = strlist_contains(seen, p->key);
	if (already) {
		long idx = pkglist_index(out, p);
		if (!pub_path || idx < 0 || out->pub[idx])
			return 0;
		out->pub[idx] = true;
	} else {
		strlist_push(seen, p->key);
	}
	strlist_push(stack, p->key);

	int rc = 0;
	bool recurse = ctx->max_depth <= 0 || depth < ctx->max_depth;
	if (recurse) {
		pkg_dep *sub;
		if (!already) {
			size_t np = pkg_parse_deps(p->requires_private_str,
						   &sub);
			for (size_t i = np; i-- > 0;)
				if (visit(ctx, &sub[i], false, depth + 1, out,
					  seen, stack, p->key) != 0)
					rc = -1;
			pkg_deps_free(sub, np);
		}

		size_t nsub = pkg_parse_deps(p->requires_str, &sub);
		for (size_t i = nsub; i-- > 0;)
			if (visit(ctx, &sub[i], pub_path, depth + 1, out, seen,
				  stack, p->key) != 0)
				rc = -1;
		pkg_deps_free(sub, nsub);
	}

	strlist_remove_at(stack, stack->len - 1);
	if (!already)
		pkglist_append(out, p, pub_path);
	return rc;
}

static int check_conflicts(pkg_ctx *ctx, const pkglist *out)
{
	int rc = 0;
	for (size_t i = 0; i < out->len; i++) {
		pkg_dep *cf;
		size_t ncf = pkg_parse_deps(out->items[i]->conflicts_str, &cf);
		for (size_t j = 0; j < ncf; j++) {
			package *other = pkglist_by_name(out, cf[j].name);
			if (other && other != out->items[i] &&
			    pkg_op_satisfied(cf[j].op, other->version,
					     cf[j].version)) {
				strbuf_addf(&ctx->errors,
					    "Package '%s' conflicts with '%s'\n",
					    out->items[i]->key, cf[j].name);
				rc = -1;
			}
		}
		pkg_deps_free(cf, ncf);
	}
	return rc;
}

int pkg_closure(pkg_ctx *ctx, const pkg_dep *roots, size_t nroots, pkglist *out)
{
	int rc = 0;
	strlist seen;
	strlist stack;
	strlist_init(&seen);
	strlist_init(&stack);
	for (size_t i = 0; i < nroots; i++)
		if (visit(ctx, &roots[i], true, 1, out, &seen, &stack,
			  nullptr) != 0)
			rc = -1;
	strlist_free(&seen);
	strlist_free(&stack);

	for (size_t i = 0; i < out->len / 2; i++) {
		size_t j = out->len - 1 - i;
		package *t = out->items[i];
		out->items[i] = out->items[j];
		out->items[j] = t;
		bool b = out->pub[i];
		out->pub[i] = out->pub[j];
		out->pub[j] = b;
	}

	if (rc == 0 && check_conflicts(ctx, out) != 0)
		rc = -1;
	return rc;
}
