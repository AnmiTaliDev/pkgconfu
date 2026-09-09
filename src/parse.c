/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"

static char *dir_of(const char *path)
{
	const char *slash = strrchr(path, '/');
	if (!slash)
		return xstrdup(".");
	if (slash == path)
		return xstrdup("/");
	return xstrndup(path, (size_t)(slash - path));
}

static void set_var(package *p, const char *name, char *value)
{
	for (size_t i = 0; i < p->nvars; i++) {
		if (strcmp(p->vars[i].name, name) == 0) {
			free(p->vars[i].value);
			p->vars[i].value = value;
			return;
		}
	}
	if (p->nvars == p->varcap) {
		p->varcap = p->varcap ? p->varcap * 2 : 8;
		p->vars = xrealloc(p->vars, p->varcap * sizeof(*p->vars));
	}
	p->vars[p->nvars].name = xstrdup(name);
	p->vars[p->nvars].value = value;
	p->nvars++;
}

const char *package_get_var(const package *p, const char *name)
{
	for (size_t i = 0; i < p->nvars; i++)
		if (strcmp(p->vars[i].name, name) == 0)
			return p->vars[i].value;
	return nullptr;
}

static const char *define_lookup(const strlist *defines, const char *name)
{
	if (!defines)
		return nullptr;
	size_t n = strlen(name);
	for (size_t i = 0; i < defines->len; i++) {
		const char *d = defines->items[i];
		if (strncmp(d, name, n) == 0 && d[n] == '=')
			return d + n + 1;
	}
	return nullptr;
}

static char *substitute(package *p, const strlist *defines, const char *in)
{
	strbuf out;
	strbuf_init(&out);
	for (const char *s = in; *s;) {
		if (s[0] == '$' && s[1] == '$') {
			strbuf_addc(&out, '$');
			s += 2;
			continue;
		}
		if (s[0] == '$' && s[1] == '{') {
			const char *end = strchr(s + 2, '}');
			if (!end) {
				strbuf_addc(&out, *s++);
				continue;
			}
			char *name = xstrndup(s + 2, (size_t)(end - (s + 2)));
			const char *val = define_lookup(defines, name);
			if (!val)
				val = package_get_var(p, name);
			if (val)
				strbuf_adds(&out, val);
			else if (!strlist_contains(&p->unresolved, name))
				strlist_push(&p->unresolved, name);
			free(name);
			s = end + 1;
			continue;
		}
		strbuf_addc(&out, *s++);
	}
	return out.data;
}

static void append_field(char **field, const char *value)
{
	if (!*field) {
		*field = xstrdup(value);
		return;
	}
	char *merged = xasprintf("%s %s", *field, value);
	free(*field);
	*field = merged;
}

package *package_parse_file(const char *path, const char *key,
			   const strlist *defines, const char *sysroot,
			   strbuf *err)
{
	FILE *f = fopen(path, "r");
	if (!f) {
		if (err)
			strbuf_addf(err, "Failed to open '%s'\n", path);
		return nullptr;
	}

	package *p = xmalloc(sizeof(*p));
	*p = (package){ 0 };
	p->key = xstrdup(key);
	p->path = xstrdup(path);

	char *pcfiledir = dir_of(path);
	set_var(p, "pcfiledir", xstrdup(pcfiledir));
	free(pcfiledir);
	set_var(p, "pc_sysrootdir",
		xstrdup(sysroot && *sysroot ? sysroot : "/"));
	set_var(p, "pc_top_builddir",
		xstrdup(getenv("PKG_CONFIG_TOP_BUILD_DIR")
				? getenv("PKG_CONFIG_TOP_BUILD_DIR")
				: "$(top_builddir)"));

	char *line = nullptr;
	size_t cap = 0;
	ssize_t n;
	while ((n = getline(&line, &cap, f)) != -1) {
		if (n && line[n - 1] == '\n')
			line[--n] = '\0';
		if (n && line[n - 1] == '\r')
			line[--n] = '\0';

		char *q = line;
		while (*q && isspace((unsigned char)*q))
			q++;
		if (*q == '\0' || *q == '#')
			continue;

		char *name = q;
		while (*q && (isalnum((unsigned char)*q) || *q == '_' ||
			      *q == '.'))
			q++;
		char *name_end = q;
		while (*q && isspace((unsigned char)*q))
			q++;
		char delim = *q;
		if (delim != ':' && delim != '=')
			continue;
		*name_end = '\0';
		q++;
		while (*q && isspace((unsigned char)*q))
			q++;
		char *value = str_trim(q);
		char *sub = substitute(p, defines, value);

		if (delim == '=') {
			set_var(p, name, sub);
			continue;
		}

		if (strcmp(name, "Name") == 0)
			append_field(&p->name, sub);
		else if (strcmp(name, "Description") == 0)
			append_field(&p->description, sub);
		else if (strcmp(name, "Version") == 0)
			append_field(&p->version, sub);
		else if (strcmp(name, "URL") == 0)
			append_field(&p->url, sub);
		else if (strcmp(name, "Cflags") == 0 ||
			 strcmp(name, "CFlags") == 0)
			append_field(&p->cflags, sub);
		else if (strcmp(name, "Cflags.private") == 0 ||
			 strcmp(name, "CFlags.private") == 0)
			append_field(&p->cflags_private, sub);
		else if (strcmp(name, "Libs") == 0)
			append_field(&p->libs, sub);
		else if (strcmp(name, "Libs.private") == 0)
			append_field(&p->libs_private, sub);
		else if (strcmp(name, "Requires") == 0)
			append_field(&p->requires_str, sub);
		else if (strcmp(name, "Requires.private") == 0)
			append_field(&p->requires_private_str, sub);
		else if (strcmp(name, "Conflicts") == 0)
			append_field(&p->conflicts_str, sub);
		else if (strcmp(name, "Provides") == 0)
			append_field(&p->provides_str, sub);
		free(sub);
	}

	free(line);
	fclose(f);
	return p;
}

void package_free(package *p)
{
	if (!p)
		return;
	free(p->key);
	free(p->path);
	free(p->name);
	free(p->description);
	free(p->version);
	free(p->url);
	free(p->cflags);
	free(p->cflags_private);
	free(p->libs);
	free(p->libs_private);
	free(p->requires_str);
	free(p->requires_private_str);
	free(p->conflicts_str);
	free(p->provides_str);
	strlist_free(&p->unresolved);
	for (size_t i = 0; i < p->nvars; i++) {
		free(p->vars[i].name);
		free(p->vars[i].value);
	}
	free(p->vars);
	free(p);
}
