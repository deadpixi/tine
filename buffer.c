#include <stdlib.h>
#include <string.h>

#include "structs.h"
#include "buffer.h"

#define SLACK 255

typedef enum{MA, PO, IL, DL, IT, DT} action;
struct JOURNAL{
    JOURNAL *prev;
    action a;
    POS p;
    size_t n;
    wchar_t s[];
};

static bool
merge(BUFFER *b, action a, POS p, const wchar_t *s, size_t n)
{
    if (b->j && b->j->a == IT && a == IT && b->j->p.l == p.l && p.c == b->j->p.c + b->j->n){
        JOURNAL *j = realloc(b->j, sizeof(JOURNAL) + (b->j->n + n + 1) * sizeof(wchar_t));
        if (!j)
            return false;
        b->j = j;
        wmemcpy(b->j->s + b->j->n, s, n);
        b->j->n += n;
        b->j->s[b->j->n] = 0;
        return true;
    }

    if (b->j && b->j->a == MA && a == MA)
      return true;
    /* XXX - we can merge more things here */

    return false;
}

static bool
push(BUFFER *b, action a, POS p, const wchar_t *s, size_t n)
{
    if (!b->canundo || merge(b, a, p, s, n))
        return true;

    JOURNAL *j = calloc(1, sizeof(JOURNAL) + (n + 1) * sizeof(wchar_t));
    if (!j)
        return false;
    j->prev = b->j;
    j->a = a;
    j->p = p;
    j->n = n;
    wmemcpy(j->s, s, n);
    b->j = j;
    return true;
}

static bool
pop(BUFFER *b)
{
    if (b->j){
        JOURNAL *j = b->j;
        b->j = j->prev;
        free(j);
        return true;
    }
    return false;
}

BUFFER *
openbuffer(void)
{
   BUFFER *b = calloc(1, sizeof(BUFFER));
   if (!b)
      return NULL;
   for (int i = 0; i < TAG_MAX; i++)
      b->tags[i].p1 = b->tags[i].p2 = pos(NONE, NONE);
   return b;
}

void
closebuffer(BUFFER *b)
{
    if (b){
        while (b->j)
            pop(b);
        for (size_t i = 0; i < b->n; i++)
            free(b->l[i].s);
        free(b->l);
        free(b);
    }
}

static bool
ensurelines(BUFFER *b, size_t n)
{
   if (b->a >= n)
      return true;
   LINE *l = realloc(b->l, (n + SLACK) * sizeof(LINE));
   if (!l)
      return false;
   b->l = l;
   b->a = n + SLACK;
   return true;
}

static bool
doinsertline(BUFFER *b, lineno l)
{
    if (!ensurelines(b, b->n + 1))
        return false;

    memmove(b->l + l + 1, b->l + l, (b->n - l) * sizeof(LINE));
    memset(b->l + l, 0, sizeof(LINE));
    b->n++;

    return b->dirty = true;
}

static bool
dodeleteline(BUFFER *b, lineno l)
{
    free(b->l[l].s);
    memmove(b->l + l, b->l + l + 1, (b->n - l - 1) * sizeof(LINE));
    b->n--;
    return b->dirty = true;
}

static bool
ensureline(LINE *l, size_t n)
{
   if (l->a >= n)
      return true;
   wchar_t *s = realloc(l->s, (n + SLACK) * sizeof(wchar_t));
   if (!s)
      return false;
   l->s = s;
   l->a = n + SLACK;
   return true;
}

static bool
doinserttext(BUFFER *b, POS p, const wchar_t *s, size_t n)
{
    if (!n)
        return true;
    LINE *l = b->l + p.l;
    b->dirty = true;
    if (p.c > l->n){
        if (!ensureline(l, p.c))
            return false;
        wmemset(l->s + l->n, L' ', p.c - l->n);
        l->n = p.c;
    }

    if (!ensureline(l, l->n + n))
        return false;
    wmemmove(l->s + p.c + n, l->s + p.c, l->n - p.c);
    wmemcpy(l->s + p.c, s, n);
    l->n += n;
    return true;
}

static bool
dodeletetext(BUFFER *b, POS p, size_t n)
{
    if (!n)
        return true;
    LINE *l = b->l + p.l;
    b->dirty = true;
    wmemmove(l->s + p.c, l->s + p.c + n, l->n - p.c - n);
    l->n -= n;
    return true;
}

bool
insertline(BUFFER *b, lineno l)
{
    if (!push(b, IL, pos(l, 0), L"", 0))
        return false;
    if (!doinsertline(b, l))
        return pop(b), false;
    return true;
}

bool
deleteline(BUFFER *b, lineno l)
{
    if (!push(b, DL, pos(l, 0), b->l[l].s, b->l[l].n))
        return false;
    if (!dodeleteline(b, l))
        return pop(b), false;
    return true;
}

bool
inserttext(BUFFER *b, POS p, const wchar_t *s, size_t n)
{
    if (!push(b, IT, p, s, n))
        return false;
    if (!doinserttext(b, p, s, n))
        return pop(b), false;
    return true;
}

bool
deletetext(BUFFER *b, POS p, size_t n)
{
    if (!push(b, DT, p, b->l[p.l].s + p.c, n))
        return false;
    if (!dodeletetext(b, p, n))
        return pop(b), false;
    return true;
}

wint_t
charat(const BUFFER *b, POS p)
{
    if (p.l >= b->n)
        return WEOF;
    if (p.c >= b->l[p.l].n)
        return L' ';
    return b->l[p.l].s[p.c];
}

bool
prev(const BUFFER *b, POS *p)
{
    if (p->l == NONE || p->c == NONE || p->l >= b->n)
        return false;
    else if (p->c)
        p->c--;
    else if (p->l){
        p->l--;
        p->c = b->l[p->l].n;
    } else
        return false;
    return true;
}

bool
next(const BUFFER *b, POS *p)
{
    if (p->l == NONE || p->c == NONE || p->l >= b->n)
        return false;
    else if (p->c < b->l[p->l].n)
        p->c++;
    else if (p->l < b->n){
        p->l++;
        p->c = 0;
    } else
        return false;
    return true;
}

bool
attop(const BUFFER *b, POS p)
{
    return !b->n || (!p.l && !p.c);
}

bool
atbot(const BUFFER *b, POS p)
{
    return !b->n || (p.l >= b->n - 1 && p.c >= b->l[b->n - 1].n);
}

bool
ateol(const BUFFER *b, POS p)
{
    return !b->n || p.l >= b->n || p.c >= b->l[p.l].n;
}

POS
pos(lineno l, colno c)
{
    POS p = {l, c};
    return p;
}

void
begin(BUFFER *b)
{
    b->nbegin++;
}

bool
commit(BUFFER *b)
{
    b->nbegin--;
    if (b->nbegin <= 0)
      b->nbegin = 0;
    return true;
}

bool
mark(BUFFER *b)
{
   if (b->nbegin)
      return true;
   return push(b, MA, b->j? b->j->p : pos(NONE, NONE), L"", 0);
}

bool
undo(BUFFER *b, POS *p)
{
    if (!b->j)
        return false;
    if (b->j->a == MA)
      pop(b);
    bool rc = true;
    while (b->j && b->j->a != MA && rc){
        switch (b->j->a){
            case IT:
                rc = dodeletetext(b, b->j->p, b->j->n);
                break;
            case DT:
                rc = doinserttext(b, b->j->p, b->j->s, b->j->n);
                break;
            case IL:
                rc = dodeleteline(b, b->j->p.l);
                break;
            case DL:
                rc  = doinsertline(b, b->j->p.l)
                   && doinserttext(b, b->j->p, b->j->s, b->j->n);
                break;
            case PO:
                break; /* just grabbing the position */
            case MA:
                break; /* never reached */
        }
        memcpy(p, &b->j->p, sizeof(POS));
        pop(b);
    }
    if (b->j && b->j->a == MA)
      pop(b);
    return rc;
}

void
enableundo(BUFFER *b)
{
   b->canundo = true;
}

void
clearundo(BUFFER *b)
{
    while (b->j)
        pop(b);
}

bool
settag(BUFFER *b, tag t, POS p1, POS p2, int v)
{
   if (t >= TAG_MAX || p1.l == NONE || p1.c == NONE || p2.l == NONE || p2.c == NONE)
      return false;
   b->tags[t].p1 = p1;
   b->tags[t].p2 = p2;
   b->tags[t].v = v;
   return true;
}

void
cleartag(BUFFER *b, tag t)
{
   if (t < TAG_MAX){
      b->tags[t].p1 = pos(NONE, NONE);
      b->tags[t].p2 = pos(NONE, NONE);
   }
}
