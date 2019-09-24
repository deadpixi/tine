#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>

char *ellipsize(const char *s, size_t l, bool right);
wchar_t *dupstr(const wchar_t *s, size_t n);
wchar_t *stows(const char *s, size_t n);
char *wstos(const wchar_t *s, size_t n);
const char *trimleft(const char *s);
bool readfile(const char *fn,
              bool (*cb)(const wchar_t *, size_t, void *),
              void *p);

#endif
