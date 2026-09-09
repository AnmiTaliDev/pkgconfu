/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

void *xmalloc(size_t n)
{
	void *p = malloc(n ? n : 1);
	if (!p) {
		fputs("pkgconfu: out of memory\n", stderr);
		exit(2);
	}
	return p;
}

void *xrealloc(void *p, size_t n)
{
	void *r = realloc(p, n ? n : 1);
	if (!r) {
		fputs("pkgconfu: out of memory\n", stderr);
		exit(2);
	}
	return r;
}

char *xstrdup(const char *s)
{
	size_t n = strlen(s) + 1;
	char *r = xmalloc(n);
	memcpy(r, s, n);
	return r;
}

char *xstrndup(const char *s, size_t n)
{
	size_t l = strnlen(s, n);
	char *r = xmalloc(l + 1);
	memcpy(r, s, l);
	r[l] = '\0';
	return r;
}

char *xasprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	va_list ap2;
	va_copy(ap2, ap);
	int n = vsnprintf(nullptr, 0, fmt, ap);
	va_end(ap);
	if (n < 0) {
		va_end(ap2);
		return xstrdup("");
	}
	char *r = xmalloc((size_t)n + 1);
	vsnprintf(r, (size_t)n + 1, fmt, ap2);
	va_end(ap2);
	return r;
}

void strbuf_init(strbuf *b)
{
	b->data = xmalloc(16);
	b->data[0] = '\0';
	b->len = 0;
	b->cap = 16;
}

void strbuf_free(strbuf *b)
{
	free(b->data);
	b->data = nullptr;
	b->len = b->cap = 0;
}

void strbuf_reset(strbuf *b)
{
	b->len = 0;
	if (b->data)
		b->data[0] = '\0';
}

static void strbuf_grow(strbuf *b, size_t extra)
{
	if (b->len + extra + 1 <= b->cap)
		return;
	while (b->len + extra + 1 > b->cap)
		b->cap *= 2;
	b->data = xrealloc(b->data, b->cap);
}

void strbuf_addc(strbuf *b, char c)
{
	strbuf_grow(b, 1);
	b->data[b->len++] = c;
	b->data[b->len] = '\0';
}

void strbuf_adds(strbuf *b, const char *s)
{
	size_t n = strlen(s);
	strbuf_grow(b, n);
	memcpy(b->data + b->len, s, n);
	b->len += n;
	b->data[b->len] = '\0';
}

void strbuf_addf(strbuf *b, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	va_list ap2;
	va_copy(ap2, ap);
	int n = vsnprintf(nullptr, 0, fmt, ap);
	va_end(ap);
	if (n < 0) {
		va_end(ap2);
		return;
	}
	strbuf_grow(b, (size_t)n);
	vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap2);
	va_end(ap2);
	b->len += (size_t)n;
}

void strlist_init(strlist *l)
{
	l->items = nullptr;
	l->len = 0;
	l->cap = 0;
}

void strlist_free(strlist *l)
{
	for (size_t i = 0; i < l->len; i++)
		free(l->items[i]);
	free(l->items);
	l->items = nullptr;
	l->len = l->cap = 0;
}

void strlist_push_owned(strlist *l, char *s)
{
	if (l->len == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 8;
		l->items = xrealloc(l->items, l->cap * sizeof(*l->items));
	}
	l->items[l->len++] = s;
}

void strlist_push(strlist *l, const char *s)
{
	strlist_push_owned(l, xstrdup(s));
}

long strlist_index(const strlist *l, const char *s)
{
	for (size_t i = 0; i < l->len; i++)
		if (strcmp(l->items[i], s) == 0)
			return (long)i;
	return -1;
}

bool strlist_contains(const strlist *l, const char *s)
{
	return strlist_index(l, s) >= 0;
}

void strlist_remove_at(strlist *l, size_t i)
{
	if (i >= l->len)
		return;
	free(l->items[i]);
	memmove(l->items + i, l->items + i + 1,
		(l->len - i - 1) * sizeof(*l->items));
	l->len--;
}

char *str_trim(char *s)
{
	while (*s && isspace((unsigned char)*s))
		s++;
	if (!*s)
		return s;
	char *end = s + strlen(s) - 1;
	while (end > s && isspace((unsigned char)*end))
		*end-- = '\0';
	return s;
}

bool str_has_prefix(const char *s, const char *prefix)
{
	return strncmp(s, prefix, strlen(prefix)) == 0;
}

size_t shell_split(const char *s, strlist *out)
{
	size_t start = out->len;
	strbuf cur;
	strbuf_init(&cur);
	bool have = false;
	while (*s) {
		unsigned char c = (unsigned char)*s;
		if (isspace(c)) {
			if (have) {
				strlist_push(out, cur.data);
				strbuf_reset(&cur);
				have = false;
			}
			s++;
			continue;
		}
		have = true;
		if (c == '\'') {
			s++;
			while (*s && *s != '\'')
				strbuf_addc(&cur, *s++);
			if (*s == '\'')
				s++;
		} else if (c == '"') {
			s++;
			while (*s && *s != '"') {
				if (*s == '\\' && (s[1] == '"' || s[1] == '\\'))
					s++;
				strbuf_addc(&cur, *s++);
			}
			if (*s == '"')
				s++;
		} else if (c == '\\') {
			if (s[1])
				strbuf_addc(&cur, s[1]), s += 2;
			else
				s++;
		} else {
			strbuf_addc(&cur, *s++);
		}
	}
	if (have)
		strlist_push(out, cur.data);
	strbuf_free(&cur);
	return out->len - start;
}
