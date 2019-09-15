#include <libgen.h>
#include <ncursesw/ncurses.h>
#include <stdbool.h>
#include <string.h>

#include "editor.h"
#include "mode.h"
#include "util.h"

#define DEFAULT_TS 3
#define DEFAULT_PH 12
static bool
initview(VIEW *v, WINDOW *w, MODE *m, void (*statuscb)(EDITOR *e, VIEW *v))
{
    v->b = openbuffer();
    if (!v->b)
        return false;
    v->w = w;
    v->m = m;
    v->bs = v->be = v->rm = NONE;
    v->lm = 0;
    v->ts = DEFAULT_TS;
    v->ph = DEFAULT_PH;
    v->statuscb = statuscb;
    v->delay = true;
    return true;
}

static void
freeview(VIEW *v)
{
    if (v){
        free(v->dl);
        closebuffer(v->b);
        if (v->w && v->w != stdscr)
            delwin(v->w);
    }
}

static void
docstatus(EDITOR *e, VIEW *v)
{
    WINDOW *w = e->cmdview.w;
    int lines, cols;

    getmaxyx(w, lines, cols); (void)lines;
    char buf[cols + 1];
    memset(buf, 0, sizeof(buf));

    char rm[25] = {0};
    if (v->rm != NONE)
        snprintf(rm, sizeof(rm) - 1, "%zu", v->rm + 1);
    else
        snprintf(rm, sizeof(rm) - 1, "?");

    if (e->err[0])
        snprintf(buf, cols, "%s", e->err);
    else{
        char *fn = ellipsize(basename(e->name), 12, false);
        snprintf(buf, cols,
         "%sFile=%-15s Line=%-5zu Col=%-5zu Block=%s%s %sTabs=%-2zu %sMargins=%zu-%s",
         v->b->dirty? "*" : " ",
         fn? fn : basename(e->name),
         v->p.l + 1, v->p.c + 1,
         v->bs == NONE? "?" : "S",
         v->be == NONE? "?" : "E",
         v->et? "*" : " ",
         v->ts,
         v->ex? "*" : " ",
         v->lm == NONE? 1 : v->lm + 1,
         rm);
         free(fn);
    }

    werase(w);
    mvwprintw(w, 0, 0, "%s", buf);
    e->err[0] = 0;
    wrefresh(w);
}

EDITOR *
openeditor(const char *name, WINDOW *docwin, WINDOW *cmdwin)
{
    EDITOR *e = calloc(1, sizeof(EDITOR));
    if (!e
    ||  !initview(&e->cmdview, cmdwin, cmdmode, NULL)
    ||  !initview(&e->docview, docwin, docmode, docstatus)){
        closeeditor(e);
        return NULL;
    }

    e->lc = pos(NONE, NONE);
    e->focusview = &e->docview;
    strncpy(e->name, name, FILENAME_MAX);
    for (int i = 0; i < CTRL_MAX; i++)
        e->ctrlmap[i] = i;
    for (int i = 0; i < BM_MAX; i++)
        e->bm[i] = pos(NONE, NONE);
    e->running = true;
    return e;
}

void
closeeditor(EDITOR *e)
{
    if (e){
        for (size_t i = 0; i < FUNC_MAX; i++)
            free(e->funcs[i]);
        free(e->find);
        freeview(&e->cmdview);
        freeview(&e->docview);
        free(e);
    }
}

static KEYSTROKE
remap(const EDITOR *e, KEYSTROKE k)
{
   if (k.o == OK && k.c < CTRL_MAX)
      k.c = e->ctrlmap[k.c];
   return k;
}

void
dispatch(EDITOR *e, VIEW *v, KEYSTROKE i)
{
    KEYSTROKE k = remap(e, i);
    ARG a = {.t = ARG_STRING, .n1 = 1, .s1 = &k.c};
    callback c = lookupkeystroke(v->m, k, &a);
    if (!c)
        return;
    c(e, v, &a);
}

static void
reframe(VIEW *v)
{
    size_t lines, cols;
    getmaxyx(v->w, lines, cols);

    if (v->p.l >= v->tos.l && v->p.l < v->tos.l + lines)
        ; /* do nothing */
    else if (v->p.l < v->tos.l && v->tos.l - v->p.l < 5)
        v->tos.l -= (v->tos.l - v->p.l);
    else if (v->p.l < lines)
        v->tos.l = 0;
    else if (v->p.l > v->tos.l + (lines - 1) && v->p.l - (v->tos.l + lines - 1) < 5)
        v->tos.l += (v->p.l - (v->tos.l + lines - 1));
    else
        v->tos.l = v->p.l - 0.33 * lines;

    if (v->p.c - v->tos.c < cols)
        ; /* do nothing */
    else if (v->p.c < v->tos.c && v->tos.c - v->p.c < 5)
        v->tos.c -= (v->tos.c - v->p.c);
    else if (v->p.c < cols)
        v->tos.c = 0;
    else if (v->p.c > v->tos.c + (cols - 1) && v->p.c - (v->tos.c + cols - 1) < 5)
        v->tos.c += (v->p.c - (v->tos.c + cols - 1));
    else
        v->tos.c = v->p.c - 0.33 * cols;
}

void
redisplay(VIEW *v)
{
    reframe(v);

    size_t lines, cols, y = 0, x = 0;
    getmaxyx(v->w, lines, cols);

    if (v->bs < v->tos.l && v->be >= v->tos.l)
      wattron(v->w, A_REVERSE);
    if (v->hls.l < v->tos.l && v->hle.l >= v->tos.l)
      wattron(v->w, A_BOLD | A_UNDERLINE);

    werase(v->w);
    for (size_t l = 0; l < lines && v->tos.l + l < v->b->n; l++){
        if (v->bs != NONE && v->be != NONE){
            if (v->tos.l + l == v->bs)
                wattron(v->w, A_REVERSE);
            else if (v->tos.l + l == v->be + 1)
                wattroff(v->w, A_REVERSE);
        }
        if (v->tos.l + l > v->hle.l)
            wattroff(v->w, A_BOLD | A_UNDERLINE);

        size_t c = 0, i = 0;
        while (c < cols){
            wmove(v->w, l, c);
            if (v->tos.l + l == v->p.l && v->tos.c + i == v->p.c)
                getyx(v->w, y, x);
            if (v->tos.l + l == v->hls.l && v->tos.c + i == v->hls.c)
                wattron(v->w, A_BOLD | A_UNDERLINE);
            if (v->tos.l + l == v->hle.l && v->tos.c + i == v->hle.c)
                wattroff(v->w, A_BOLD | A_UNDERLINE);
            wchar_t w = charat(v->b, pos(v->tos.l + l, v->tos.c + i));
            if (w == '\t'){
                c++;
                while (c % v->ts)
                    c++;
            } else{
                int cw = wcwidth(w) > 0? wcwidth(w) : 0;
                wchar_t s[] = {w, 0, 0};
                if (iswcntrl(w)){
                  s[0] = L'^';
                  s[1] = L'@' + w;
                  if (wcwidth(s[1]) <= 0)
                     s[1] = L'?';
                  cw = 1 + wcwidth(s[1]);
                }
                waddwstr(v->w, s);
                c+= cw;
            }
            i++;
        }
    }
    wattroff(v->w, A_BOLD | A_UNDERLINE | A_REVERSE);
    wmove(v->w, y, x);
    wrefresh(v->w);
}

bool
error(EDITOR *e, const char *s)
{
    strncpy(e->err, s, ERR_MAX);
    return false;
}

KEYSTROKE
getkeystroke(EDITOR *e, bool delay)
{
    if (delay != e->focusview->delay){
        nodelay(e->focusview->w, !delay);
        e->focusview->delay = delay;
    }

    KEYSTROKE k = {0};
    wint_t c = 0;
    int o = wget_wch(e->focusview->w, &c);
    while (o == KEY_CODE_YES && c == KEY_RESIZE){
        redisplay(&e->docview);
        if (e->focusview->statuscb)
            e->focusview->statuscb(e, e->focusview);
        redisplay(e->focusview);
        o = wget_wch(e->focusview->w, &c);
    }
    k.o = c == WEOF? ERR : o;
    k.c = c;
    return k;
}

static void
fixviewcursor(VIEW *v)
{
    if (v->p.l >= v->b->n)
        v->p.l = v->b->n? v->b->n - 1 : 0;
}

void
fixcursor(EDITOR *e)
{
    fixviewcursor(&e->docview);
    fixviewcursor(&e->cmdview);
}

static bool
pbefore(POS p1, POS p2)
{
    return p1.l < p2.l || (p1.l == p2.l && p1.c < p2.c);
}

void
hilight(VIEW *v, POS p1, POS p2)
{
    if (pbefore(p1, p2)){
        v->hls = p1;
        v->hle = p2;
    } else{
        v->hls = p2;
        v->hle = p1;
    }
    redisplay(v);
}

void
clearhilight(VIEW *v)
{
    v->hls = pos(NONE, NONE);
    v->hle = pos(NONE, NONE);
    redisplay(v);
}
