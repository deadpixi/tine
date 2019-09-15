#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "editor.h"
#include "command.h"

#define DEPTH_MAX 32
typedef struct STATE STATE;
struct STATE{
    const wchar_t *s;
    size_t n, o, d;
    EDITOR *e;
};

static bool chain(STATE *s);

static void
skipws(STATE *s)
{
    while (s->o < s->n && iswspace(s->s[s->o]))
        s->o++;
}

static wint_t
consume(STATE *s, bool skip)
{
    if (skip)
        skipws(s);
    return s->o >= s->n? WEOF : (wint_t)s->s[s->o++];
}

static wint_t
consume1(STATE *s)
{
    return consume(s, false);
}

static wint_t
peek(STATE *s, bool skip)
{
    if (skip)
        skipws(s);
    return s->o >= s->n? WEOF : (wint_t)s->s[s->o];
}

static wint_t
peek1(STATE *s)
{
    return peek(s, false);
}

static bool
name(STATE *s, wchar_t *b)
{
    wmemset(b, 0, CMD_NAME_MAX);
    for (int i = 0; i < CMD_NAME_MAX && iswalpha(peek1(s)); i++)
        b[i] = towupper(consume1(s));
    return b[0];
}

static bool
string(STATE *s, ARG *a)
{
    wint_t d = consume(s, true);
    if (d == WEOF || iswalnum(d) || d == L';' || d == L'(' || d == L')')
        return error(s->e, "Invalid string delimiter");

    a->s1 = s->s + s->o;
    wint_t c = consume1(s);
    while (c != WEOF && c != d){
        a->n1++;
        c = consume1(s);
    }
    a->t = ARG_STRING;
    return true;
}

static bool
exchange(STATE *s, ARG *a)
{
    ARG a2 = {0};
    if (!string(s, a))
        return false;
    s->o--;
    if (!string(s, &a2))
        return false;
    a->n2 = a2.n1;
    a->s2 = a2.s1;
    a->t = ARG_EXCHANGE;
    return true;
}

static size_t
number(STATE *s)
{
    size_t r = 0;
    if (!iswdigit(peek(s, true)))
        return error(s->e, "Number expected"), (size_t)-1;
    skipws(s);
    while (iswdigit(peek1(s))){
        wint_t c = consume1(s);
        r *= 10;
        r += c - L'0';
    }
    return r;
}

static bool
numarg(STATE *s, ARG *a)
{
    a->n1 = number(s);
    if (a->n1 == (size_t)-1)
        return false;
    a->t = ARG_NUMBER;
    return true;
}

static bool
argavail(STATE *s)
{
    wint_t c = peek(s, true);
    return c != WEOF && c != L';' && c != L'(' && c != L')';
}

static bool
simplecommand(STATE *s)
{
    wchar_t n[CMD_NAME_MAX + 1] = {0};
    if (!name(s, n))
        return error(s->e, "Name expected");
    const CMD *c = lookup(n);
    if (!c)
        return error(s->e, "Unknown command");
    ARG a = {0};
    if (argavail(s)){
        switch (c->a){
            case ARG_NONE:                                         break;
            case ARG_STRING:   if (!string(s, &a))   return false; break;
            case ARG_EXCHANGE: if (!exchange(s, &a)) return false; break;
            case ARG_NUMBER:   if (!numarg(s, &a))   return false; break;
        }
    } else if (c->required && c->a != ARG_NONE)
        return error(s->e, "Argument expected");
    return call(c, s->e, &s->e->docview, &a);
}

static size_t
count(STATE *s)
{
    if (iswdigit(peek(s, true)))
        return number(s);

    STATE t = *s; /* simulate extra lookahead */
    wchar_t b[CMD_NAME_MAX + 1] = {0};
    if (name(&t, b) && wcscmp(b, L"RP") == 0){
        memcpy(s, &t, sizeof(t)); /* actually take the lookahead */
        return SIZE_MAX;
    }
    return 1;
}

static bool
command(STATE *s)
{
    skipws(s);

    size_t n = count(s);
    STATE t = {0};
    for (size_t i = 0; i < n; i++){
        memcpy(&t, s, sizeof(t));
        if (peek(&t, true) == L'('){
            consume(&t, true);
            if (s->d++ > DEPTH_MAX)
                return error(s->e, "?FORMULA TOO COMPLEX");
            if (!chain(&t))
                return memcpy(s, &t, sizeof(t)), false;
            if (consume(&t, true) != L')')
                return memcpy(s, &t, sizeof(t)), error(s->e, "Unmatched();");
            s->d--;
        } else if (!simplecommand(&t))
            return memcpy(s, &t, sizeof(t)), false;

        redisplay(&s->e->docview);
        if (n > 1){
            KEYSTROKE k = getkeystroke(s->e, false);
            if (k.o != ERR)
                return error(s->e, "Commands abandoned");
        }
    }
    memcpy(s, &t, sizeof(t));
    return true;
}

static bool
chain(STATE *s)
{
    if (!command(s))
        return false;
    while (peek(s, true) == L';'){
        consume(s, true);
        if (!command(s))
            return false;
    }
    return true;
}

bool
runextended(const wchar_t *c, size_t n, EDITOR *e)
{
    STATE s = {.s = c, .n = n? n : wcslen(c), .o = 0, .d = 0, .e = e};
    mark(e->docview.b);
    begin(e->docview.b);
    bool rc = chain(&s);
    commit(e->docview.b);
    if (!rc)
        return false;
    if (peek(&s, true) != WEOF)
        return error(e, "Extra input at end");
    return true;
}
