#ifndef EDITOR_H
#define EDITOR_H

#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>
#include CURSES_INCLUDE

#include "buffer.h"
#include "mode.h"

struct VIEW{
    BUFFER *b;
    POS p, tos, gb;
    WINDOW *w;
    MODE *m;
    lineno bs, be;
    void (*statuscb)(EDITOR *e, VIEW *v);
    bool ex, uc, delay, et, q, ai, sm, se;
    size_t ph, ts, lm, rm, sd;
    wchar_t *dl;
    size_t dln;
};

#define FUNC_MAX 10
#define CTRL_MAX 32
#define ERR_MAX 127
#define BM_MAX 10
struct EDITOR{
    bool running, needsresize;
    VIEW cmdview, docview, *focusview;
    POS bm[BM_MAX], lc;
    wchar_t *funcs[FUNC_MAX];
    char ctrlmap[CTRL_MAX];
    char name[FILENAME_MAX + 1];
    char err[ERR_MAX + 1];
    wchar_t *find;
    size_t findn;
};

EDITOR *openeditor(const char *name, WINDOW *docwin, WINDOW *cmdwin);
void closeeditor(EDITOR *e);

void dispatch(EDITOR *e, VIEW *v, KEYSTROKE k);
void redisplay(VIEW *v);

void hilight(VIEW *v, POS p1, POS p2);
void clearhilight(VIEW *v);
bool error(EDITOR *e, const char *s);

KEYSTROKE getkeystroke(EDITOR *e, bool delay);
void fixcursor(EDITOR *e);

#endif
