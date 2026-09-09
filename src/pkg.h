/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PKGCONFU_PKG_H
#define PKGCONFU_PKG_H

#include "parse.h"
#include "util.h"

typedef enum {
	CMP_ANY,
	CMP_LT,
	CMP_LE,
	CMP_EQ,
	CMP_NE,
	CMP_GE,
	CMP_GT,
} cmp_op;

typedef struct {
	char *name;
	cmp_op op;
	char *version;
} pkg_dep;

typedef struct {
	strlist path;
	strlist defines;
	char *sysroot;
	int max_depth;
	bool disable_uninstalled;
	bool define_prefix;
	const char *prefix_var;
	package **loaded;
	size_t nloaded;
	size_t loadcap;
	bool provides_indexed;
	strlist alias_name;
	package **alias_pkg;
	size_t nalias;
	size_t aliascap;
	strbuf errors;
} pkg_ctx;

typedef struct {
	package **items;
	bool *pub;
	size_t len;
	size_t cap;
} pkglist;

void pkg_ctx_init(pkg_ctx *ctx);
void pkg_ctx_free(pkg_ctx *ctx);

void pkg_add_path(pkg_ctx *ctx, const char *dir);
void pkg_add_path_list(pkg_ctx *ctx, const char *list);
void pkg_default_paths(pkg_ctx *ctx);

package *pkg_load(pkg_ctx *ctx, const char *name);

int pkg_vercmp(const char *a, const char *b);
bool pkg_op_satisfied(cmp_op op, const char *have, const char *want);
const char *pkg_op_name(cmp_op op);

size_t pkg_parse_deps(const char *s, pkg_dep **out);
void pkg_deps_free(pkg_dep *d, size_t n);

void pkglist_free(pkglist *l);
void pkglist_append(pkglist *l, package *p, bool pub);
int pkg_closure(pkg_ctx *ctx, const pkg_dep *roots, size_t nroots,
		pkglist *out);

#endif
