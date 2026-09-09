/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"
#include "util.h"

#ifndef PKGCONFU_VERSION
#define PKGCONFU_VERSION "0.2.0"
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
	bool print_requires;
	bool print_requires_private;
	bool print_provides;
	bool validate;
	bool list_all;
	bool help;
	bool version;
	bool keep_system_cflags;
	bool keep_system_libs;
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
		"  --print-requires              list Requires entries\n"
		"  --print-requires-private      list Requires.private entries\n"
		"  --print-provides              list what the packages provide\n"
		"  --validate                    check package files for problems\n"
		"  --list-all                    list all known packages\n"
		"  --with-path=DIR               prepend DIR to the search path\n"
		"  --maximum-traverse-depth=N    limit dependency recursion depth\n"
		"  --keep-system-cflags          keep -I/usr/include\n"
		"  --keep-system-libs            keep -L/usr/lib\n"
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

static char *apply_sysroot(const char *t, const char *sysroot)
{
	if (!sysroot || !*sysroot || strcmp(sysroot, "/") == 0)
		return xstrdup(t);
	if ((str_has_prefix(t, "-I") || str_has_prefix(t, "-L")) && t[2] == '/') {
		if (str_has_prefix(t + 2, sysroot))
			return xstrdup(t);
		return xasprintf("-%c%s%s", t[1], sysroot, t + 2);
	}
	return xstrdup(t);
}

static void collect(const pkglist *pkgs, bool libs, bool static_mode,
		    enum out_filter filter, const char *sysroot,
		    bool keep_system, strlist *out)
{
	strlist raw;
	strlist_init(&raw);
	for (size_t i = 0; i < pkgs->len; i++) {
		package *p = pkgs->items[i];
		if (libs) {
			if (p->libs)
				shell_split(p->libs, &raw);
			if (static_mode && p->libs_private)
				shell_split(p->libs_private, &raw);
		} else {
			if (p->cflags)
				shell_split(p->cflags, &raw);
			if (static_mode && p->cflags_private)
				shell_split(p->cflags_private, &raw);
		}
	}

	strlist norm;
	strlist_init(&norm);
	normalize(&raw, &norm);

	for (size_t i = 0; i < norm.len; i++) {
		const char *t = norm.items[i];
		if (!pass_filter(t, filter))
			continue;
		char *v = apply_sysroot(t, sysroot);
		if (!keep_system && is_system_flag(v, libs)) {
			free(v);
			continue;
		}
		if (str_has_prefix(v, "-I") || str_has_prefix(v, "-L")) {
			if (strlist_contains(out, v))
				free(v);
			else
				strlist_push_owned(out, v);
		} else {
			long at = strlist_index(out, v);
			if (at >= 0)
				strlist_remove_at(out, (size_t)at);
			strlist_push_owned(out, v);
		}
	}

	strlist_free(&raw);
	strlist_free(&norm);
}

static void print_dep_list(const char *s)
{
	pkg_dep *d;
	size_t n = pkg_parse_deps(s, &d);
	for (size_t i = 0; i < n; i++) {
		if (d[i].op == CMP_ANY)
			puts(d[i].name);
		else
			printf("%s %s %s\n", d[i].name, pkg_op_name(d[i].op),
			       d[i].version ? d[i].version : "");
	}
	pkg_deps_free(d, n);
}

static int validate_pkg(const package *p)
{
	int bad = 0;
	if (!p->name || !*p->name) {
		fprintf(stderr, "%s: missing Name\n", p->key);
		bad = 1;
	}
	if (!p->description)
		fprintf(stderr, "%s: missing Description\n", p->key);
	if (!p->version || !*p->version) {
		fprintf(stderr, "%s: missing Version\n", p->key);
		bad = 1;
	}
	for (size_t i = 0; i < p->unresolved.len; i++) {
		fprintf(stderr, "%s: undefined variable '%s'\n", p->key,
			p->unresolved.items[i]);
		bad = 1;
	}
	return bad;
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
	strlist with_paths;
	strlist_init(&defines);
	strlist_init(&with_paths);
	strbuf reqbuf;
	strbuf_init(&reqbuf);
	int max_depth = 0;

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
		else if (strcmp(a, "--print-requires") == 0)
			o.print_requires = true;
		else if (strcmp(a, "--print-requires-private") == 0)
			o.print_requires_private = true;
		else if (strcmp(a, "--print-provides") == 0)
			o.print_provides = true;
		else if (strcmp(a, "--validate") == 0)
			o.validate = true;
		else if (strcmp(a, "--list-all") == 0)
			o.list_all = true;
		else if (strcmp(a, "--keep-system-cflags") == 0)
			o.keep_system_cflags = true;
		else if (strcmp(a, "--keep-system-libs") == 0)
			o.keep_system_libs = true;
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
		else if (str_has_prefix(a, "--with-path=") && eq)
			strlist_push(&with_paths, eq + 1);
		else if (str_has_prefix(a, "--maximum-traverse-depth=") && eq)
			max_depth = atoi(eq + 1);
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
			strlist_free(&with_paths);
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
		strlist_free(&with_paths);
		strbuf_free(&reqbuf);
		return 0;
	}
	if (o.version) {
		puts(PKGCONFU_VERSION);
		strlist_free(&defines);
		strlist_free(&with_paths);
		strbuf_free(&reqbuf);
		return 0;
	}

	pkg_ctx ctx;
	pkg_ctx_init(&ctx);
	ctx.max_depth = max_depth;
	const char *sysroot = getenv("PKG_CONFIG_SYSROOT_DIR");
	if (sysroot && *sysroot) {
		size_t n = strlen(sysroot);
		while (n > 1 && sysroot[n - 1] == '/')
			n--;
		ctx.sysroot = xstrndup(sysroot, n);
	}
	for (size_t i = 0; i < defines.len; i++)
		strlist_push(&ctx.defines, defines.items[i]);
	strlist_free(&defines);

	for (size_t i = 0; i < with_paths.len; i++)
		pkg_add_path_list(&ctx, with_paths.items[i]);
	strlist_free(&with_paths);
	const char *env_path = getenv("PKG_CONFIG_PATH");
	if (env_path && *env_path)
		pkg_add_path_list(&ctx, env_path);
	pkg_default_paths(&ctx);

	bool keep_cflags = o.keep_system_cflags ||
			   getenv("PKG_CONFIG_ALLOW_SYSTEM_CFLAGS");
	bool keep_libs = o.keep_system_libs ||
			 getenv("PKG_CONFIG_ALLOW_SYSTEM_LIBS");

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

	pkglist pk = { 0 };
	int rc = 0;

	bool simple_action = o.modversion || o.variable || o.print_variables ||
			     o.print_requires || o.print_requires_private ||
			     o.print_provides || o.validate;
	bool need_closure = check_only || o.cflags || o.libs ||
			    (!simple_action);

	if (need_closure)
		rc |= pkg_closure(&ctx, roots, nroots, &pk);

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
	if (rc != 0) {
		if (show_errors)
			fputs(ctx.errors.data, errf);
		ret = 1;
		pkglist_free(&pk);
		pkg_deps_free(roots, nroots);
		goto done;
	}

	if (o.validate) {
		for (size_t i = 0; i < nroots; i++) {
			package *p = pkg_load(&ctx, roots[i].name);
			if (p && validate_pkg(p))
				ret = 1;
		}
	}

	if (o.modversion) {
		for (size_t i = 0; i < nroots; i++) {
			package *p = pkg_load(&ctx, roots[i].name);
			puts(p && p->version ? p->version : "");
		}
	}

	if (o.print_provides) {
		for (size_t i = 0; i < nroots; i++) {
			package *p = pkg_load(&ctx, roots[i].name);
			if (!p)
				continue;
			printf("%s = %s\n", p->key,
			       p->version ? p->version : "");
			print_dep_list(p->provides_str);
		}
	}

	if (o.print_requires) {
		for (size_t i = 0; i < nroots; i++) {
			package *p = pkg_load(&ctx, roots[i].name);
			if (p)
				print_dep_list(p->requires_str);
		}
	}

	if (o.print_requires_private) {
		for (size_t i = 0; i < nroots; i++) {
			package *p = pkg_load(&ctx, roots[i].name);
			if (p)
				print_dep_list(p->requires_private_str);
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
		strlist outc;
		strlist outl;
		strlist_init(&outc);
		strlist_init(&outl);
		if (o.cflags)
			collect(&pk, false, o.static_mode, o.cflags_filter,
				ctx.sysroot, keep_cflags, &outc);
		if (o.libs) {
			pkglist lv = { 0 };
			for (size_t i = 0; i < pk.len; i++)
				if (pk.pub[i] || o.static_mode)
					pkglist_append(&lv, pk.items[i],
						       pk.pub[i]);
			collect(&lv, true, o.static_mode, o.libs_filter,
				ctx.sysroot, keep_libs, &outl);
			pkglist_free(&lv);
		}
		strlist joined;
		strlist_init(&joined);
		for (size_t i = 0; i < outc.len; i++)
			strlist_push(&joined, outc.items[i]);
		for (size_t i = 0; i < outl.len; i++)
			strlist_push(&joined, outl.items[i]);
		emit(&joined);
		strlist_free(&joined);
		strlist_free(&outc);
		strlist_free(&outl);
	}

	pkglist_free(&pk);
	pkg_deps_free(roots, nroots);

done:
	strbuf_free(&reqbuf);
	pkg_ctx_free(&ctx);
	return ret;
}
