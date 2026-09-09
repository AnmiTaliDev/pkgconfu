/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"
#include "util.h"

#ifndef PKGCONFU_VERSION
#define PKGCONFU_VERSION "0.1.0"
#endif
#define PKGCONFU_PKGCONFIG_COMPAT "0.29.2"

enum out_filter {
	F_ALL,
	F_I,
	F_L,
	F_L_CAP,
	F_OTHER,
};

typedef struct {
	bool cflags;
	bool libs;
	enum out_filter cflags_filter;
	enum out_filter libs_filter;
	bool modversion;
	bool exists;
	bool static_mode;
	bool print_variables;
	bool list_all;
	bool help;
	bool version;
	const char *variable;
	const char *atleast_version;
	const char *exact_version;
	const char *max_version;
	const char *atleast_pkgconfig_version;
	bool print_errors;
	bool silence_errors;
	bool errors_to_stdout;
} options;

static void usage(FILE *f)
{
	fputs(
		"Usage: pkgconfu [OPTIONS] PACKAGE...\n"
		"\n"
		"  --version                     print pkgconfu version\n"
		"  --modversion                  print module version\n"
		"  --exists                      return success if packages exist\n"
		"  --cflags                      print compile flags\n"
		"  --cflags-only-I               print only -I flags\n"
		"  --cflags-only-other           print non -I compile flags\n"
		"  --libs                        print link flags\n"
		"  --libs-only-l                 print only -l flags\n"
		"  --libs-only-L                 print only -L flags\n"
		"  --libs-only-other             print non -l/-L link flags\n"
		"  --static                      output for static linking\n"
		"  --variable=NAME               print value of a variable\n"
		"  --define-variable=NAME=VALUE  set a variable\n"
		"  --print-variables             list defined variables\n"
		"  --list-all                    list all known packages\n"
		"  --atleast-version=VERSION     require at least this version\n"
		"  --exact-version=VERSION       require exactly this version\n"
		"  --max-version=VERSION         require at most this version\n"
		"  --atleast-pkgconfig-version=VERSION  require pkg-config compat level\n"
		"  --print-errors                print errors on failure\n"
		"  --silence-errors              suppress error output\n"
		"  --errors-to-stdout            print errors to stdout\n"
		"  --help                        this message\n",
		f);
}

static void print_quoted(const char *t)
{
	if (!strpbrk(t, " \t\n\"'\\")) {
		fputs(t, stdout);
		return;
	}
	putchar('"');
	for (const char *p = t; *p; p++) {
		if (*p == '"' || *p == '\\')
			putchar('\\');
		putchar(*p);
	}
	putchar('"');
}

static void emit(const strlist *toks)
{
	for (size_t i = 0; i < toks->len; i++) {
		if (i)
			putchar(' ');
		print_quoted(toks->items[i]);
	}
	if (toks->len)
		putchar('\n');
}

static void normalize(const strlist *raw, strlist *norm)
{
	for (size_t i = 0; i < raw->len; i++) {
		const char *t = raw->items[i];
		if ((strcmp(t, "-I") == 0 || strcmp(t, "-L") == 0 ||
		     strcmp(t, "-l") == 0) &&
		    i + 1 < raw->len) {
			strlist_push_owned(norm,
					   xasprintf("%s%s", t,
						     raw->items[++i]));
		} else {
			strlist_push(norm, t);
		}
	}
}

static bool pass_filter(const char *t, enum out_filter f)
{
	bool is_i = str_has_prefix(t, "-I");
	bool is_l = str_has_prefix(t, "-l");
	bool is_lcap = str_has_prefix(t, "-L");
	switch (f) {
	case F_ALL:
		return true;
	case F_I:
		return is_i;
	case F_L:
		return is_l;
	case F_L_CAP:
		return is_lcap;
	case F_OTHER:
		return !is_i && !is_l && !is_lcap;
	}
	return true;
}

static bool is_system_flag(const char *t, bool libs)
{
	if (libs)
		return strcmp(t, "-L/usr/lib") == 0 ||
		       strcmp(t, "-L/usr/lib64") == 0;
	return strcmp(t, "-I/usr/include") == 0;
}

static void collect(const pkglist *pkgs, bool libs, bool static_mode,
		    enum out_filter filter, strlist *out)
{
	bool allow_system = getenv(libs ? "PKG_CONFIG_ALLOW_SYSTEM_LIBS"
					: "PKG_CONFIG_ALLOW_SYSTEM_CFLAGS") !=
			    nullptr;
	strlist raw;
	strlist_init(&raw);
	for (size_t i = 0; i < pkgs->len; i++) {
		package *p = pkgs->items[i];
		if (libs) {
			if (p->libs)
				shell_split(p->libs, &raw);
			if (static_mode && p->libs_private)
				shell_split(p->libs_private, &raw);
		} else if (p->cflags) {
			shell_split(p->cflags, &raw);
		}
	}

	strlist norm;
	strlist_init(&norm);
	normalize(&raw, &norm);

	for (size_t i = 0; i < norm.len; i++) {
		const char *t = norm.items[i];
		if (!pass_filter(t, filter))
			continue;
		if (!allow_system && is_system_flag(t, libs))
			continue;
		if (str_has_prefix(t, "-l")) {
			long at = strlist_index(out, t);
			if (at >= 0)
				strlist_remove_at(out, (size_t)at);
			strlist_push(out, t);
		} else if (!strlist_contains(out, t)) {
			strlist_push(out, t);
		}
	}

	strlist_free(&raw);
	strlist_free(&norm);
}

static int cmp_rows(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int list_all(pkg_ctx *ctx)
{
	strlist rows;
	strlist_init(&rows);
	strlist seen;
	strlist_init(&seen);

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
			if (strlist_contains(&seen, mod)) {
				free(mod);
				continue;
			}
			strlist_push_owned(&seen, mod);
			package *p = pkg_load(ctx, seen.items[seen.len - 1]);
			if (p)
				strlist_push_owned(
					&rows,
					xasprintf("%-30s %s - %s", p->key,
						  p->name ? p->name : p->key,
						  p->description
							  ? p->description
							  : ""));
		}
		closedir(d);
	}

	qsort(rows.items, rows.len, sizeof(*rows.items), cmp_rows);
	for (size_t i = 0; i < rows.len; i++)
		puts(rows.items[i]);

	strlist_free(&rows);
	strlist_free(&seen);
	return 0;
}

int main(int argc, char **argv)
{
	options o = { 0 };
	strlist defines;
	strlist_init(&defines);
	strbuf reqbuf;
	strbuf_init(&reqbuf);

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		const char *eq = strchr(a, '=');
		if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0)
			o.help = true;
		else if (strcmp(a, "--version") == 0)
			o.version = true;
		else if (strcmp(a, "--modversion") == 0)
			o.modversion = true;
		else if (strcmp(a, "--exists") == 0)
			o.exists = true;
		else if (strcmp(a, "--cflags") == 0)
			o.cflags = true;
		else if (strcmp(a, "--cflags-only-I") == 0)
			o.cflags = true, o.cflags_filter = F_I;
		else if (strcmp(a, "--cflags-only-other") == 0)
			o.cflags = true, o.cflags_filter = F_OTHER;
		else if (strcmp(a, "--libs") == 0)
			o.libs = true;
		else if (strcmp(a, "--libs-only-l") == 0)
			o.libs = true, o.libs_filter = F_L;
		else if (strcmp(a, "--libs-only-L") == 0)
			o.libs = true, o.libs_filter = F_L_CAP;
		else if (strcmp(a, "--libs-only-other") == 0)
			o.libs = true, o.libs_filter = F_OTHER;
		else if (strcmp(a, "--static") == 0)
			o.static_mode = true;
		else if (strcmp(a, "--print-variables") == 0)
			o.print_variables = true;
		else if (strcmp(a, "--list-all") == 0)
			o.list_all = true;
		else if (strcmp(a, "--print-errors") == 0)
			o.print_errors = true;
		else if (strcmp(a, "--silence-errors") == 0)
			o.silence_errors = true;
		else if (strcmp(a, "--errors-to-stdout") == 0)
			o.errors_to_stdout = true;
		else if (str_has_prefix(a, "--variable=") && eq)
			o.variable = eq + 1;
		else if (str_has_prefix(a, "--define-variable=") && eq)
			strlist_push(&defines, eq + 1);
		else if (str_has_prefix(a, "--atleast-version=") && eq)
			o.atleast_version = eq + 1;
		else if (str_has_prefix(a, "--exact-version=") && eq)
			o.exact_version = eq + 1;
		else if (str_has_prefix(a, "--max-version=") && eq)
			o.max_version = eq + 1;
		else if (str_has_prefix(a, "--atleast-pkgconfig-version=") && eq)
			o.atleast_pkgconfig_version = eq + 1;
		else if (str_has_prefix(a, "--")) {
			fprintf(stderr, "pkgconfu: unknown option '%s'\n", a);
			strlist_free(&defines);
			strbuf_free(&reqbuf);
			return 1;
		} else {
			if (reqbuf.len)
				strbuf_addc(&reqbuf, ' ');
			strbuf_adds(&reqbuf, a);
		}
	}

	if (o.help) {
		usage(stdout);
		strlist_free(&defines);
		strbuf_free(&reqbuf);
		return 0;
	}
	if (o.version) {
		puts(PKGCONFU_VERSION);
		strlist_free(&defines);
		strbuf_free(&reqbuf);
		return 0;
	}

	pkg_ctx ctx;
	pkg_ctx_init(&ctx);
	for (size_t i = 0; i < defines.len; i++)
		strlist_push(&ctx.defines, defines.items[i]);
	strlist_free(&defines);

	const char *env_path = getenv("PKG_CONFIG_PATH");
	if (env_path && *env_path)
		pkg_add_path_list(&ctx, env_path);
	pkg_default_paths(&ctx);

	bool check_only = o.exists || o.atleast_version || o.exact_version ||
			  o.max_version;
	bool show_errors = o.print_errors ||
			   !(check_only || o.atleast_pkgconfig_version);
	if (o.silence_errors)
		show_errors = false;
	FILE *errf = o.errors_to_stdout ? stdout : stderr;

	int ret = 0;

	if (o.atleast_pkgconfig_version) {
		ret = pkg_vercmp(PKGCONFU_PKGCONFIG_COMPAT,
				 o.atleast_pkgconfig_version) >= 0
			      ? 0
			      : 1;
		goto done;
	}

	if (o.list_all) {
		ret = list_all(&ctx);
		goto done;
	}

	pkg_dep *roots;
	size_t nroots = pkg_parse_deps(reqbuf.data, &roots);
	if (nroots == 0) {
		if (show_errors)
			fputs("pkgconfu: no package names specified\n", errf);
		ret = 1;
		goto done;
	}

	cmp_op override = CMP_ANY;
	const char *override_ver = nullptr;
	if (o.atleast_version)
		override = CMP_GE, override_ver = o.atleast_version;
	else if (o.exact_version)
		override = CMP_EQ, override_ver = o.exact_version;
	else if (o.max_version)
		override = CMP_LE, override_ver = o.max_version;
	if (override != CMP_ANY) {
		for (size_t i = 0; i < nroots; i++) {
			roots[i].op = override;
			free(roots[i].version);
			roots[i].version = xstrdup(override_ver);
		}
	}

	pkglist libs_pk = { 0 };
	pkglist cflags_pk = { 0 };
	int rc = 0;

	bool simple_action = o.modversion || o.variable || o.print_variables;
	bool need_closure = check_only ||
			    (!o.cflags && !o.libs && !simple_action);

	if (need_closure) {
		pkglist tmp = { 0 };
		rc |= pkg_closure(&ctx, roots, nroots, true, &tmp);
		pkglist_free(&tmp);
	}
	if (simple_action) {
		for (size_t i = 0; i < nroots; i++) {
			package *p = pkg_load(&ctx, roots[i].name);
			if (!p) {
				strbuf_addf(&ctx.errors,
					    "Package '%s' was not found in the pkg-config search path\n",
					    roots[i].name);
				rc = -1;
			} else if (roots[i].op != CMP_ANY &&
				   !pkg_op_satisfied(roots[i].op, p->version,
						     roots[i].version)) {
				strbuf_addf(&ctx.errors,
					    "Requested '%s %s %s' but version of %s is %s\n",
					    roots[i].name,
					    pkg_op_name(roots[i].op),
					    roots[i].version ? roots[i].version
							     : "",
					    p->name ? p->name : roots[i].name,
					    p->version ? p->version : "");
				rc = -1;
			}
		}
	}
	if (o.cflags)
		rc |= pkg_closure(&ctx, roots, nroots, true, &cflags_pk);
	if (o.libs)
		rc |= pkg_closure(&ctx, roots, nroots, o.static_mode, &libs_pk);

	if (rc != 0) {
		if (show_errors)
			fputs(ctx.errors.data, errf);
		ret = 1;
		pkglist_free(&libs_pk);
		pkglist_free(&cflags_pk);
		pkg_deps_free(roots, nroots);
		goto done;
	}

	if (o.modversion) {
		for (size_t i = 0; i < nroots; i++) {
			package *p = pkg_load(&ctx, roots[i].name);
			puts(p && p->version ? p->version : "");
		}
	}

	if (o.variable) {
		strbuf line;
		strbuf_init(&line);
		for (size_t i = 0; i < nroots; i++) {
			package *p = pkg_load(&ctx, roots[i].name);
			const char *v = p ? package_get_var(p, o.variable)
					  : nullptr;
			if (v && *v) {
				if (line.len)
					strbuf_addc(&line, ' ');
				strbuf_adds(&line, v);
			}
		}
		puts(line.data);
		strbuf_free(&line);
	}

	if (o.print_variables) {
		for (size_t i = 0; i < nroots; i++) {
			package *p = pkg_load(&ctx, roots[i].name);
			if (!p)
				continue;
			for (size_t j = 0; j < p->nvars; j++)
				puts(p->vars[j].name);
		}
	}

	if (o.cflags || o.libs) {
		strlist out;
		strlist_init(&out);
		if (o.cflags)
			collect(&cflags_pk, false, false, o.cflags_filter,
				&out);
		if (o.libs)
			collect(&libs_pk, true, o.static_mode, o.libs_filter,
				&out);
		emit(&out);
		strlist_free(&out);
	}

	pkglist_free(&libs_pk);
	pkglist_free(&cflags_pk);
	pkg_deps_free(roots, nroots);

done:
	strbuf_free(&reqbuf);
	pkg_ctx_free(&ctx);
	return ret;
}
