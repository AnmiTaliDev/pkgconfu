/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PKGCONFU_PARSE_H
#define PKGCONFU_PARSE_H

#include "util.h"

typedef struct {
	char *name;
	char *value;
} pkg_var;

typedef struct package {
	char *key;
	char *path;

	char *name;
	char *description;
	char *version;
	char *url;

	char *cflags;
	char *libs;
	char *libs_private;

	char *requires_str;
	char *requires_private_str;
	char *conflicts_str;

	pkg_var *vars;
	size_t nvars;
	size_t varcap;
} package;

package *package_parse_file(const char *path, const char *key,
			   const strlist *defines, strbuf *err);
void package_free(package *p);
const char *package_get_var(const package *p, const char *name);

#endif
