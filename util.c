#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>

#include "util.h"

char *
ellipsize(const char *s, size_t l, bool right)
{
   size_t n = strlen(s);
   if (n <= l)
      return strdup(s);
   char *o = calloc(l + 3 + 1, sizeof(char));
   if (!o)
      return NULL;

   if (!right){
      strcat(o, "...");
      strncat(o, s + n - l, l);
   } else{
      strncat(o, s + n - l, l);
      strcat(o, "...");
   }

   return o;
}

wchar_t *
stows(const char *s, size_t n)
{
    char *mbs = calloc(n + 1, sizeof(char));
    if (!mbs)
        return NULL;
    strncpy(mbs, s, n);

    size_t wcn = mbstowcs(NULL, mbs, 0);
    wchar_t *wcs = calloc(wcn + 1, sizeof(wchar_t));
    if (!wcs)
        return free(mbs), NULL;

    size_t wcr = mbstowcs(wcs, mbs, wcn);
    if (wcr == (size_t)-1)
        return free(mbs), free(wcs), NULL;

    free(mbs);
    return wcs;
}

char *
wstos(const wchar_t *s, size_t n)
{
    wchar_t *wcs = calloc(n + 1, sizeof(wchar_t));
    if (!wcs)
        return NULL;
    wcsncpy(wcs, s, n);

    size_t mbn = wcstombs(NULL, wcs, 0);
    char *mbs = calloc(mbn + 1, sizeof(char));
    if (!mbs)
        return free(wcs), NULL;

    size_t mbr = wcstombs(mbs, wcs, mbn);
    if (mbr == (size_t)-1)
        return free(mbs), free(wcs), NULL;

    free(wcs);
    return mbs;
}

bool
readfile(const char *fn, bool (*cb)(const wchar_t *, size_t, void *), void *p)
{
    struct stat s = {0};
    if (stat(fn, &s) != 0)
       return false;
    if ((s.st_mode & S_IFMT) == S_IFDIR)
       return (errno = EISDIR), false;

    FILE *f = fopen(fn, "r");
    if (!f)
        return false;

    char *l = NULL;
    size_t ln = 0;
    ssize_t n = 0;
    bool rc = true;
    while ((n = getline(&l, &ln, f)) != -1 && rc){
        if (n && l[n - 1] == '\n')
            n--;
        if (n){
            wchar_t *c = stows(l, n);
            rc = c && cb(c, wcslen(c), p);
            free(c);
        } else
            rc = cb(L"", 0, p);
    }
    free(l);
    fclose(f);
    return rc;
}

wchar_t *
dupstr(const wchar_t *s, size_t n)
{
    wchar_t *r = calloc(n + 1, sizeof(wchar_t));
    if (!r)
        return r;
    wmemcpy(r, s, n);
    return r;
}
