/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PKGCONFU_UTIL_H
#define PKGCONFU_UTIL_H

#include <stddef.h>

void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);
[[gnu::format(printf, 1, 2)]] char *xasprintf(const char *fmt, ...);

typedef struct {
	char *data;
	size_t len;
	size_t cap;
} strbuf;

void strbuf_init(strbuf *b);
void strbuf_free(strbuf *b);
void strbuf_reset(strbuf *b);
void strbuf_addc(strbuf *b, char c);
void strbuf_adds(strbuf *b, const char *s);
[[gnu::format(printf, 2, 3)]] void strbuf_addf(strbuf *b, const char *fmt, ...);

typedef struct {
	char **items;
	size_t len;
	size_t cap;
} strlist;

void strlist_init(strlist *l);
void strlist_free(strlist *l);
void strlist_push(strlist *l, const char *s);
void strlist_push_owned(strlist *l, char *s);
bool strlist_contains(const strlist *l, const char *s);
long strlist_index(const strlist *l, const char *s);
void strlist_remove_at(strlist *l, size_t i);

char *str_trim(char *s);
bool str_has_prefix(const char *s, const char *prefix);

size_t shell_split(const char *s, strlist *out);

#endif
